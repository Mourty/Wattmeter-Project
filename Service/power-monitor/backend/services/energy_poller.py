import asyncio
import httpx
import logging
from datetime import datetime
from typing import Dict, Optional
from models import Meter, EnergyReading
from services.database import Database

logger = logging.getLogger(__name__)

class EnergyPoller:
    def __init__(self, database: Database):
        self.db = database
        self.meters: Dict[str, Meter] = {}
        self.tasks: Dict[str, asyncio.Task] = {}
        self.running = False
        self.client = None

    async def start(self):
        """Start energy polling for all registered meters"""
        self.running = True
        # Configure with connection limits and keep-alive
        limits = httpx.Limits(
            max_connections=50,
            max_keepalive_connections=20,
            keepalive_expiry=30.0
        )
        self.client = httpx.AsyncClient(
            timeout=10.0,  # Energy polling can be slower
            limits=limits,
            transport=httpx.AsyncHTTPTransport(retries=0)
        )

        # Load meters from database
        meters = await self.db.get_all_meters()
        for meter_data in meters:
            meter = Meter(**meter_data)
            if meter.enabled:
                await self.add_meter(meter)

        logger.info(f"Started energy polling for {len(self.meters)} meters")

    async def stop(self):
        """Stop all energy polling tasks"""
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

        logger.info("Stopped all energy polling")

    async def add_meter(self, meter: Meter):
        """Add a meter and start energy polling it"""
        self.meters[meter.meter_id] = meter

        # Start energy polling task if enabled
        if meter.enabled and self.running:
            task = asyncio.create_task(self._poll_energy(meter))
            self.tasks[meter.meter_id] = task
            logger.info(f"Started energy polling for meter {meter.meter_id} at {meter.ip_address}")

    async def remove_meter(self, meter_id: str):
        """Remove a meter and stop energy polling it"""
        if meter_id in self.tasks:
            self.tasks[meter_id].cancel()
            try:
                await self.tasks[meter_id]
            except asyncio.CancelledError:
                pass
            del self.tasks[meter_id]

        if meter_id in self.meters:
            del self.meters[meter_id]
            logger.info(f"Removed energy polling for meter {meter_id}")

    async def update_meter(self, meter_id: str, meter: Meter):
        """Update a meter's energy polling configuration"""
        # Remove old task
        await self.remove_meter(meter_id)

        # Add with new configuration
        await self.add_meter(meter)

    async def _poll_energy(self, meter: Meter):
        """Continuously poll energy data from a single meter"""
        consecutive_failures = 0
        max_failures = 5

        while self.running and meter.enabled:
            try:
                # Fetch energy data from meter
                readings = await self._fetch_energy_data(meter)

                if readings:
                    # Store all phase readings in database
                    for reading in readings:
                        await self.db.store_energy_reading(reading)
                    consecutive_failures = 0
                    logger.debug(f"Stored {len(readings)} energy readings from {meter.meter_id}")
                else:
                    consecutive_failures += 1
                    logger.warning(f"Failed to read energy from {meter.meter_id} ({consecutive_failures}/{max_failures})")

            except Exception as e:
                consecutive_failures += 1
                logger.error(f"Error polling energy from {meter.meter_id}: {e}")

            # Back off if too many failures
            if consecutive_failures >= max_failures:
                logger.error(f"Energy polling for {meter.meter_id} has failed {consecutive_failures} times, backing off")
                await asyncio.sleep(60)  # Wait 60 seconds before retrying
                consecutive_failures = 0
            else:
                await asyncio.sleep(meter.energy_poll_interval)

    async def _fetch_energy_data(self, meter: Meter) -> Optional[list]:
        """Fetch energy data from a single meter via HTTP"""
        try:
            # Query all phases
            url = f"http://{meter.ip_address}/api/energy?phase=ALL"

            response = await self.client.get(url)
            response.raise_for_status()

            data = response.json()
            timestamp = datetime.utcnow()  # Use Pi's timestamp

            # Check if request was successful
            if not data.get('success', False):
                logger.error(f"Meter {meter.meter_id} energy endpoint returned success=false")
                return None

            readings = []

            # Handle response - could be single phase or multiple phases
            if isinstance(data, dict) and 'phase' in data:
                # Single phase response
                phase = data.get('phase')
                total_kwh = data.get('accumulatedKWh', 0.0)

                reading = EnergyReading(
                    timestamp=timestamp,
                    meter_id=meter.meter_id,
                    phase=phase,
                    total_kwh=total_kwh
                )
                readings.append(reading)
            elif isinstance(data, dict) and 'phases' in data:
                # Multiple phases response
                for phase_data in data.get('phases', []):
                    phase = phase_data.get('phase')
                    total_kwh = phase_data.get('accumulatedKWh', 0.0)

                    reading = EnergyReading(
                        timestamp=timestamp,
                        meter_id=meter.meter_id,
                        phase=phase,
                        total_kwh=total_kwh
                    )
                    readings.append(reading)
            else:
                logger.warning(f"Unexpected energy response format from {meter.meter_id}")
                return None

            return readings if readings else None

        except httpx.HTTPError as e:
            logger.error(f"HTTP error fetching energy from {meter.meter_id}: {e}")
            return None
        except KeyError as e:
            logger.error(f"Missing expected field in energy response from {meter.meter_id}: {e}")
            return None
        except Exception as e:
            logger.error(f"Unexpected error fetching energy from {meter.meter_id}: {e}")
            return None
