import asyncio
import httpx
import logging
from datetime import datetime
from typing import Dict, Optional
from models import Meter, MeterReading
from services.database import Database

logger = logging.getLogger(__name__)

class MeterPoller:
    def __init__(self, database: Database):
        self.db = database
        self.meters: Dict[str, Meter] = {}
        self.tasks: Dict[str, asyncio.Task] = {}
        self.poll_interval = 1.0
        self.running = False
        self.client = None
    
    async def start(self):
        """Start polling all registered meters"""
        self.running = True
        # Configure with connection limits and keep-alive
        limits = httpx.Limits(
            max_connections=50,          # Total connection pool size
            max_keepalive_connections=20, # Reuse connections
            keepalive_expiry=30.0        # Close idle connections after 30s
        )
        self.client = httpx.AsyncClient(
            timeout=5.0,
            limits=limits,
            transport=httpx.AsyncHTTPTransport(retries=0)  # No automatic retries
        )
        
        # Load meters from database
        meters = await self.db.get_all_meters()
        for meter_data in meters:
            meter = Meter(**meter_data)
            if meter.enabled:
                await self.add_meter(meter)
        
        logger.info(f"Started polling {len(self.meters)} meters")
    
    async def stop(self):
        """Stop all polling tasks"""
        self.running = False
        
        # Cancel all tasks
        for task in self.tasks.values():
            task.cancel()
        
        # Wait for all tasks to complete
        if self.tasks:
            await asyncio.gather(*self.tasks.values(), return_exceptions=True)
        
        # Close HTTP client
        if self.client:
            await self.client.aclose()
        
        logger.info("Stopped all meter polling")
    
    async def add_meter(self, meter: Meter):
        """Add a meter and start polling it"""
        self.meters[meter.meter_id] = meter
        
        # Start polling task if enabled
        if meter.enabled and self.running:
            task = asyncio.create_task(self._poll_meter(meter))
            self.tasks[meter.meter_id] = task
            logger.info(f"Started polling meter {meter.meter_id} at {meter.ip_address}")
    
    async def remove_meter(self, meter_id: str):
        """Remove a meter and stop polling it"""
        if meter_id in self.tasks:
            self.tasks[meter_id].cancel()
            try:
                await self.tasks[meter_id]
            except asyncio.CancelledError:
                pass
            del self.tasks[meter_id]
        
        if meter_id in self.meters:
            del self.meters[meter_id]
            logger.info(f"Removed meter {meter_id}")
    
    async def update_meter(self, meter_id: str, meter: Meter):
        """Update a meter's configuration"""
        # Remove old task
        await self.remove_meter(meter_id)
        
        # Add with new configuration
        await self.add_meter(meter)
    
    async def _poll_meter(self, meter: Meter):
        """Continuously poll a single meter"""
        consecutive_failures = 0
        max_failures = 5
        
        while self.running and meter.enabled:
            try:
                # Fetch data from meter
                reading = await self._fetch_meter_data(meter)
                
                if reading:
                    # Store in database
                    await self.db.store_reading(reading)
                    consecutive_failures = 0
                    logger.debug(f"Stored reading from {meter.meter_id}: {reading.active_power}W, PF={reading.power_factor:.3f}")
                else:
                    consecutive_failures += 1
                    logger.warning(f"Failed to read from {meter.meter_id} ({consecutive_failures}/{max_failures})")
                
            except Exception as e:
                consecutive_failures += 1
                logger.error(f"Error polling {meter.meter_id}: {e}")
            
            # Back off if too many failures
            if consecutive_failures >= max_failures:
                logger.error(f"Meter {meter.meter_id} has failed {consecutive_failures} times, backing off")
                await asyncio.sleep(30)  # Wait 30 seconds before retrying
                consecutive_failures = 0
            else:
                await asyncio.sleep(meter.poll_interval)
    
    async def _fetch_meter_data(self, meter: Meter) -> Optional[MeterReading]:
        """Fetch data from a single meter via HTTP"""
        try:
            # Use your ESP32's /api/read endpoint with POST
            url = f"http://{meter.ip_address}/api/read"
            
            # Request the registers we need including power factor
            payload = {
                "registers": [
                    "UrmsA",     # Phase A Voltage RMS
                    "IrmsA",     # Phase A Current RMS  
                    "PmeanA",    # Phase A Active Power
                    "QmeanA",    # Phase A Reactive Power
                    "SmeanA",    # Phase A Apparent Power
                    "PFmeanA",       # Phase A Power Factor
                    "Freq"       # Frequency
                ]
            }
            
            response = await self.client.post(url, json=payload)
            response.raise_for_status()
            
            data = response.json()
            timestamp = datetime.utcnow()
            
            # Check if request was successful
            if not data.get('success', False):
                logger.error(f"Meter {meter.meter_id} returned success=false")
                return None
            
            # Parse the data array into a dictionary for easier access
            values = {}
            for item in data.get('data', []):
                if 'error' not in item:
                    values[item['name']] = item['value']
            
            # Get power factor, or calculate it if not available from meter
            power_factor = values.get('PFA', 0.0)
            
            # If PFA wasn't available, try calculating from active/apparent power
            if power_factor == 0.0:
                apparent = values.get('SmeanA', 0.0)
                if apparent > 0:
                    power_factor = values.get('PmeanA', 0.0) / apparent
                else:
                    power_factor = 0.0
            
            # Create the reading with the values
            reading = MeterReading(
                timestamp=timestamp,
                meter_id=meter.meter_id,
                voltage_rms=values.get('UrmsA', 0.0),
                current_rms=values.get('IrmsA', 0.0),
                active_power=values.get('PmeanA', 0.0),
                reactive_power=values.get('QmeanA', 0.0),
                apparent_power=values.get('SmeanA', 0.0),
                power_factor=power_factor,
                frequency=values.get('Freq', 0.0)
            )
            
            return reading
            
        except httpx.HTTPError as e:
            logger.error(f"HTTP error fetching data from {meter.meter_id}: {e}")
            return None
        except KeyError as e:
            logger.error(f"Missing expected field in response from {meter.meter_id}: {e}")
            return None
        except Exception as e:
            logger.error(f"Unexpected error fetching data from {meter.meter_id}: {e}")
            return None