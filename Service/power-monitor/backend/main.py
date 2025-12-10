from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import Response
from contextlib import asynccontextmanager
import asyncio
from datetime import datetime, timedelta
from typing import List, Optional
import logging

from models import (Meter, MeterReading, MeterConfig, HistoricalQuery, SystemSettings,
                   EnergyReading, EnergyCalibrationStart, EnergyCalibrationComplete, EnergyHistoricalQuery)
from services.database import Database
from services.meter_poller import MeterPoller
from services.energy_poller import EnergyPoller
from services.websocket_manager import WebSocketManager
import psutil
import os

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Global instances
db = Database()
poller = MeterPoller(db)
energy_poller = EnergyPoller(db)
ws_manager = WebSocketManager()


async def maintenance_task():
    """Periodic maintenance task with smart space management"""
    while True:
        try:
            await asyncio.sleep(3600)  # Run every hour
            
            # Checkpoint WAL
            await db.checkpoint_wal()
            
            # Get stats
            stats = await db.get_database_stats()
            if stats:
                logger.info(f"DB: {stats['total_size_mb']}MB, "
                          f"Readings: {stats['reading_count']:,}, "
                          f"Disk: {stats['disk_free_gb']:.2f}GB free ({stats['disk_percent_used']:.1f}% used)")
                
                # Warn if WAL is getting too large (>100MB)
                if stats['wal_size_mb'] > 100:
                    logger.warning(f"WAL file is large ({stats['wal_size_mb']}MB), running VACUUM...")
                    await db.conn.execute("VACUUM")
                    await db.checkpoint_wal()
                
                # Clean up only if disk space is getting low
                settings = await db.get_settings()
                min_free_gb = settings.get('min_free_space_gb', 1.0)  # Default 1GB minimum
                
                if stats['disk_free_gb'] < min_free_gb:
                    logger.warning(f"Low disk space ({stats['disk_free_gb']:.2f}GB). Starting cleanup...")
                    
                    # First, try aggregating very old data (older than 90 days)
                    aggregated = await db.aggregate_old_data(days_old=90, aggregation_hours=1)
                    logger.info(f"Aggregated {aggregated} old readings to hourly averages")
                    
                    # If still low on space, delete oldest data
                    stats = await db.get_database_stats()
                    if stats['disk_free_gb'] < min_free_gb:
                        deleted = await db.cleanup_oldest_data(
                            target_free_space_gb=min_free_gb + 0.5,  # Try to get 0.5GB buffer
                            batch_days=30
                        )
                        logger.info(f"Deleted {deleted} oldest readings to free space")
                
                # Log data age range
                if stats['oldest_reading'] and stats['newest_reading']:
                    oldest = datetime.fromisoformat(stats['oldest_reading'].replace('Z', '+00:00'))
                    newest = datetime.fromisoformat(stats['newest_reading'].replace('Z', '+00:00'))
                    age_days = (newest - oldest).days
                    logger.info(f"Data spans {age_days} days (from {oldest.date()} to {newest.date()})")
                    
        except Exception as e:
            logger.error(f"Error in maintenance task: {e}")            




@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    await db.initialize()
    await poller.start()
    await energy_poller.start()

    # Start maintenance task
    maintenance = asyncio.create_task(maintenance_task())

    logger.info("Power Monitor Backend Started")
    yield

    # Shutdown
    maintenance.cancel()
    try:
        await maintenance
    except asyncio.CancelledError:
        pass

    await poller.stop()
    await energy_poller.stop()
    await db.close()
    logger.info("Power Monitor Backend Stopped")

app = FastAPI(title="Power Monitor API", version="1.0.0", lifespan=lifespan)

# CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Health check
@app.get("/health")
async def health_check():
    return {"status": "healthy", "timestamp": datetime.utcnow().isoformat()}

@app.get("/health/detailed")
async def detailed_health_check():
    """Detailed health check with resource info"""
    import os
    
    stats = await db.get_database_stats()
    
    try:
        with open('/proc/self/status', 'r') as f:
            for line in f:
                if line.startswith('VmRSS:'):
                    # VmRSS is resident memory in kB
                    memory_kb = int(line.split()[1])
                    memory_mb = round(memory_kb / 1024, 2)
                    break
            else:
                memory_mb = 0
    except:
        memory_mb = 0
    
    return {
        "status": "healthy",
        "timestamp": datetime.utcnow().isoformat(),
        "memory_mb": memory_mb,
        "database": stats,
        "active_meters": len(poller.meters),
        "active_tasks": len(poller.tasks)
    }


@app.get("/health/resources")
async def resource_health():
    """Check system resource usage"""
    
    try:
        # Memory info
        memory = psutil.virtual_memory()
        
        # Disk info
        disk = psutil.disk_usage('/')
        
        # Network connections
        connections = len(psutil.net_connections())
        
        # Open files
        process = psutil.Process(os.getpid())
        open_files = len(process.open_files())
        
        # Check for concerning levels
        warnings = []
        if memory.percent > 80:
            warnings.append(f"High memory usage: {memory.percent}%")
        if disk.percent > 90:
            warnings.append(f"Low disk space: {disk.free / (1024**3):.1f}GB free")
        if connections > 500:
            warnings.append(f"High connection count: {connections}")
        if open_files > 500:
            warnings.append(f"High open file count: {open_files}")
        
        return {
            "status": "warning" if warnings else "healthy",
            "warnings": warnings,
            "memory_percent": round(memory.percent, 1),
            "memory_available_mb": round(memory.available / (1024**2)),
            "disk_free_gb": round(disk.free / (1024**3), 2),
            "disk_percent": round(disk.percent, 1),
            "active_connections": connections,
            "open_files": open_files,
            "websocket_clients": sum(len(clients) for clients in ws_manager.active_connections.values())
        }
    except Exception as e:
        logger.error(f"Error getting resource health: {e}")
        return {"status": "error", "error": str(e)}


# Meter Discovery/Registration
@app.post("/api/meters/register")
async def register_meter(meter: Meter):
    """Endpoint for meters to self-register"""
    await db.register_meter(meter)
    await poller.add_meter(meter)
    await energy_poller.add_meter(meter)
    logger.info(f"Meter registered: {meter.meter_id} at {meter.ip_address}")
    return {"status": "registered", "meter_id": meter.meter_id}

# Meter Management
@app.get("/api/meters", response_model=List[Meter])
async def get_meters():
    """Get all registered meters"""
    return await db.get_all_meters()

@app.get("/api/meters/{meter_id}")
async def get_meter(meter_id: str):
    """Get specific meter info"""
    meter = await db.get_meter(meter_id)
    if not meter:
        raise HTTPException(status_code=404, detail="Meter not found")
    return meter

@app.put("/api/meters/{meter_id}")
async def update_meter(meter_id: str, meter: Meter):
    """Update meter configuration"""
    await db.update_meter(meter_id, meter)
    await poller.update_meter(meter_id, meter)
    await energy_poller.update_meter(meter_id, meter)
    return {"status": "updated", "meter_id": meter_id}

@app.delete("/api/meters/{meter_id}")
async def delete_meter(meter_id: str):
    """Remove a meter"""
    await db.delete_meter(meter_id)
    await poller.remove_meter(meter_id)
    await energy_poller.remove_meter(meter_id)
    return {"status": "deleted", "meter_id": meter_id}

# Live Data
@app.get("/api/meters/{meter_id}/latest")
async def get_latest_reading(meter_id: str):
    """Get the most recent reading for a meter"""
    reading = await db.get_latest_reading(meter_id)
    if not reading:
        raise HTTPException(status_code=404, detail="No readings found")
    return reading

@app.websocket("/api/meters/{meter_id}/live")
async def websocket_live_data(websocket: WebSocket, meter_id: str):
    """WebSocket endpoint for live data streaming"""
    await ws_manager.connect(websocket, meter_id)
    try:
        while True:
            reading = await db.get_latest_reading(meter_id)
            if reading:
                await websocket.send_json(reading)
            await asyncio.sleep(1)
    except WebSocketDisconnect:
        logger.info(f"WebSocket disconnected for meter {meter_id}")
    except Exception as e:
        logger.error(f"WebSocket error for meter {meter_id}: {e}")
    finally:
        ws_manager.disconnect(websocket, meter_id)
        logger.debug(f"WebSocket cleaned up for meter {meter_id}")


@app.post("/api/meters/{meter_id}/count")
async def get_reading_count(meter_id: str, query: HistoricalQuery):
    """Get the count of readings in a time range without fetching the data"""
    count = await db.get_reading_count(
        meter_id,
        query.start_time,
        query.end_time
    )
    return {
        "meter_id": meter_id,
        "start_time": query.start_time,
        "end_time": query.end_time,
        "count": count
    }

# Historical Data
@app.post("/api/meters/{meter_id}/historical")
async def get_historical_data(meter_id: str, query: HistoricalQuery):
    """Query historical data with time range and optional aggregation"""
    result = await db.get_historical_readings(
        meter_id,
        query.start_time,
        query.end_time,
        query.limit,
        query.aggregation
    )

    logger.info(f"Historical query for {meter_id}: {len(result['readings'])} readings, "
                f"aggregation={query.aggregation} -> {result['aggregation_applied']}, "
                f"took {result['query_time_seconds']:.2f}s")

    return {
        "meter_id": meter_id,
        "start_time": query.start_time,
        "end_time": query.end_time,
        "count": len(result['readings']),
        "readings": result['readings'],
        "aggregation_applied": result['aggregation_applied'],
        "original_count": result.get('original_count'),
        "query_time_seconds": result['query_time_seconds']
    }

# Export Data
@app.get("/api/meters/{meter_id}/export")
async def export_data(
    meter_id: str,
    start_time: Optional[str] = None,
    end_time: Optional[str] = None,
    format: str = "csv"
):
    """Export data to CSV"""
    from fastapi.responses import StreamingResponse
    import io
    import csv
    
    start = datetime.fromisoformat(start_time.replace('Z', '+00:00')) if start_time else datetime.utcnow() - timedelta(days=30)
    end = datetime.fromisoformat(end_time.replace('Z', '+00:00')) if end_time else datetime.utcnow()
    
    readings = await db.get_historical_readings(meter_id, start, end, limit=None)
    
    # Create CSV
    output = io.StringIO()
    
    # Include all fields from the readings
    if readings and len(readings) > 0:
        fieldnames = list(readings[0].keys())
    else:
        fieldnames = [
            'timestamp', 'meter_id', 'voltage_rms', 'current_rms', 'active_power',
            'reactive_power', 'apparent_power', 'power_factor', 'frequency'
        ]
    
    writer = csv.DictWriter(output, fieldnames=fieldnames)
    writer.writeheader()
    writer.writerows(readings)
    
    output.seek(0)
    
    # Get meter name for filename
    meter = await db.get_meter(meter_id)
    meter_name = meter.get('name', meter_id) if meter else meter_id
    filename = f"{meter_name}_{start.strftime('%Y%m%d')}_{end.strftime('%Y%m%d')}.csv"
    
    return StreamingResponse(
        iter([output.getvalue()]),
        media_type="text/csv",
        headers={"Content-Disposition": f"attachment; filename={filename}"}
    )
    
@app.get("/api/meters/{meter_id}/energy/export")
async def export_energy_data(
    meter_id: str,
    start_time: Optional[str] = None,
    end_time: Optional[str] = None,
    phase: str = "A"
):
    """Export energy data to CSV"""
    from fastapi.responses import StreamingResponse
    import io
    import csv

    start = datetime.fromisoformat(start_time.replace('Z', '+00:00')) if start_time else datetime.utcnow() - timedelta(days=30)
    end = datetime.fromisoformat(end_time.replace('Z', '+00:00')) if end_time else datetime.utcnow()

    # Get energy readings
    readings = await db.get_energy_readings(meter_id, start, end, phase)

    # Create CSV
    output = io.StringIO()

    # Define fieldnames for energy data
    if readings and len(readings) > 0:
        fieldnames = list(readings[0].keys())
    else:
        fieldnames = ['timestamp', 'meter_id', 'phase', 'total_kwh']

    writer = csv.DictWriter(output, fieldnames=fieldnames)
    writer.writeheader()
    writer.writerows(readings)

    output.seek(0)

    # Get meter name for filename
    meter = await db.get_meter(meter_id)
    meter_name = meter.get('name', meter_id) if meter else meter_id
    phase_suffix = f"_phase{phase}" if phase != "ALL" else "_all_phases"
    filename = f"{meter_name}_energy{phase_suffix}_{start.strftime('%Y%m%d')}_{end.strftime('%Y%m%d')}.csv"

    return StreamingResponse(
        iter([output.getvalue()]),
        media_type="text/csv",
        headers={"Content-Disposition": f"attachment; filename={filename}"}
    )

@app.api_route("/api/meters/{meter_id}/proxy/{path:path}", methods=["GET", "POST", "PUT", "DELETE"])
async def proxy_to_meter(meter_id: str, path: str, request: Request):
    """Proxy requests to the meter's ESP32 to avoid CORS issues"""
    meter = await db.get_meter(meter_id)
    if not meter:
        raise HTTPException(status_code=404, detail="Meter not found")
    
    import httpx
    
    # Build the target URL
    target_url = f"http://{meter['ip_address']}/{path}"
    
    # Get request body if present
    body = None
    if request.method in ["POST", "PUT"]:
        body = await request.body()
    
    # Forward the request
    async with httpx.AsyncClient(timeout=10.0) as client:
        try:
            response = await client.request(
                method=request.method,
                url=target_url,
                content=body,
                headers={"Content-Type": "application/json"} if body else None
            )
            
            # Return the response
            return Response(
                content=response.content,
                status_code=response.status_code,
                media_type="application/json"
            )
        except Exception as e:
            raise HTTPException(status_code=500, detail=f"Error connecting to meter: {str(e)}")

# Settings
@app.get("/api/settings")
async def get_settings():
    """Get system settings"""
    return await db.get_settings()

@app.put("/api/settings")
async def update_settings(settings: SystemSettings):
    """Update system settings"""
    await db.update_settings(settings)
    # Update poller interval if changed
    if settings.poll_interval:
        poller.poll_interval = settings.poll_interval
    return {"status": "updated"}

# Statistics
@app.get("/api/meters/{meter_id}/stats")
async def get_statistics(meter_id: str, hours: int = 24):
    """Get statistics for the last N hours"""
    end_time = datetime.utcnow()
    start_time = end_time - timedelta(hours=hours)
    
    stats = await db.get_statistics(meter_id, start_time, end_time)
    return stats


@app.get("/api/meters/{meter_id}/live-direct")
async def get_live_direct(meter_id: str):
    """Fetch data directly from meter in real-time (not from database)"""
    meter = await db.get_meter(meter_id)
    if not meter:
        raise HTTPException(status_code=404, detail="Meter not found")
    
    # Import the fetch function
    from services.meter_poller import MeterPoller
    import httpx
    
    # Create temporary client and fetch directly
    async with httpx.AsyncClient(timeout=2.0) as client:
        try:
            url = f"http://{meter['ip_address']}/api/read"
            payload = {
                "registers": ["UrmsA", "IrmsA", "PmeanA", "QmeanA", "SmeanA", "PFA", "Freq"]
            }
            
            response = await client.post(url, json=payload)
            response.raise_for_status()
            data = response.json()
            
            if not data.get('success', False):
                raise HTTPException(status_code=500, detail="Meter returned error")
            
            # Parse values
            values = {}
            for item in data.get('data', []):
                if 'error' not in item:
                    values[item['name']] = item['value']
            
            # Get power factor, or calculate it if not available
            power_factor = values.get('PFA', 0.0)
            if power_factor == 0.0:
                apparent = values.get('SmeanA', 0.0)
                if apparent > 0:
                    power_factor = values.get('PmeanA', 0.0) / apparent
            
            return {
                "timestamp": datetime.utcnow().isoformat(),
                "meter_id": meter_id,
                "voltage_rms": values.get('UrmsA', 0.0),
                "current_rms": values.get('IrmsA', 0.0),
                "active_power": values.get('PmeanA', 0.0),
                "reactive_power": values.get('QmeanA', 0.0),
                "apparent_power": values.get('SmeanA', 0.0),
                "power_factor": power_factor,
                "frequency": values.get('Freq', 0.0)
            }
            
        except Exception as e:
            raise HTTPException(status_code=500, detail=f"Error fetching from meter: {str(e)}")

@app.get("/api/database/stats")
async def get_database_stats():
    """Get database and disk space statistics"""
    stats = await db.get_database_stats()
    return stats

@app.post("/api/database/aggregate")
async def aggregate_old_data(days_old: int = 90):
    """Manually trigger data aggregation"""
    deleted = await db.aggregate_old_data(days_old=days_old)
    return {"status": "success", "aggregated_records": deleted}

@app.post("/api/database/vacuum")
async def vacuum_database():
    """Manually trigger database vacuum to reclaim space"""
    await db.checkpoint_wal()
    await db.conn.execute("VACUUM")
    return {"status": "success", "message": "Database vacuumed"}

# Energy Endpoints
@app.get("/api/meters/{meter_id}/energy/latest")
async def get_latest_energy_reading(meter_id: str, phase: Optional[str] = "ALL"):
    """Get the most recent energy reading for a meter and phase"""
    reading = await db.get_latest_energy_reading(meter_id, phase)
    if not reading:
        raise HTTPException(status_code=404, detail="No energy readings found")
    return reading

@app.post("/api/meters/{meter_id}/energy/historical")
async def get_historical_energy_data(meter_id: str, query: EnergyHistoricalQuery):
    """Query historical energy data with time range, phase filter, and aggregation"""
    result = await db.get_energy_aggregated(
        meter_id,
        query.start_time,
        query.end_time,
        query.phase,
        query.aggregation
    )

    logger.info(f"Energy historical query for {meter_id}: {result['raw_readings_count']} raw readings, "
                f"phase={query.phase}, aggregation={query.aggregation} -> {result['aggregation_applied']}, "
                f"took {result['query_time_seconds']:.2f}s")

    return {
        "meter_id": meter_id,
        "start_time": query.start_time,
        "end_time": query.end_time,
        "phase": query.phase,
        "aggregation_applied": result['aggregation_applied'],
        "raw_total_kwh": result['raw_total_kwh'],
        "aggregated": result['aggregated'],
        "count": len(result['aggregated']),
        "query_time_seconds": result['query_time_seconds']
    }

@app.post("/api/meters/{meter_id}/energy/calibrate/start")
async def start_energy_calibration(meter_id: str, calibration: EnergyCalibrationStart):
    """Start energy calibration on the ESP32"""
    meter = await db.get_meter(meter_id)
    if not meter:
        raise HTTPException(status_code=404, detail="Meter not found")

    import httpx

    # Forward to ESP32
    target_url = f"http://{meter['ip_address']}/api/energy/calibrate/start"

    async with httpx.AsyncClient(timeout=10.0) as client:
        try:
            response = await client.post(
                target_url,
                json={"phases": calibration.phases},
                headers={"Content-Type": "application/json"}
            )
            response.raise_for_status()
            return response.json()
        except Exception as e:
            raise HTTPException(status_code=500, detail=f"Error starting calibration: {str(e)}")

@app.post("/api/meters/{meter_id}/energy/calibrate/complete")
async def complete_energy_calibration(meter_id: str, calibration: EnergyCalibrationComplete):
    """Complete energy calibration on the ESP32"""
    meter = await db.get_meter(meter_id)
    if not meter:
        raise HTTPException(status_code=404, detail="Meter not found")

    import httpx

    # Forward to ESP32
    target_url = f"http://{meter['ip_address']}/api/energy/calibrate/complete"

    async with httpx.AsyncClient(timeout=10.0) as client:
        try:
            response = await client.post(
                target_url,
                json={
                    "phase": calibration.phase,
                    "loadWatts": calibration.loadWatts,
                    "durationMinutes": calibration.durationMinutes
                },
                headers={"Content-Type": "application/json"}
            )
            response.raise_for_status()
            return response.json()
        except Exception as e:
            raise HTTPException(status_code=500, detail=f"Error completing calibration: {str(e)}")