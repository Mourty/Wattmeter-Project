import aiosqlite
import json
import os
from datetime import datetime, timedelta
from typing import List, Optional, Dict, Any
import logging

from models import Meter, MeterReading, SystemSettings, AggregationType

logger = logging.getLogger(__name__)

class Database:
    def __init__(self, db_path: str = None):
        self.db_path = db_path or os.getenv("DATABASE_PATH", "/app/data/power_monitor.db")
        self.conn = None
    
    async def initialize(self):
        """Initialize database and create tables"""
        self.conn = await aiosqlite.connect(self.db_path)
        self.conn.row_factory = aiosqlite.Row
        
        # Enable WAL mode for better concurrent access
        await self.conn.execute("PRAGMA journal_mode=WAL")
        await self.conn.execute("PRAGMA synchronous=NORMAL")
        await self.conn.execute("PRAGMA busy_timeout=5000")
        
        # Create tables
        await self.conn.execute("""
            CREATE TABLE IF NOT EXISTS meters (
                meter_id TEXT PRIMARY KEY,
                ip_address TEXT NOT NULL,
                name TEXT,
                location TEXT,
                enabled INTEGER DEFAULT 1,
                poll_interval REAL DEFAULT 1.0,
                last_seen TIMESTAMP,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        """)
        
        await self.conn.execute("""
            CREATE TABLE IF NOT EXISTS readings (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TIMESTAMP NOT NULL,
                meter_id TEXT NOT NULL,
                voltage_rms REAL NOT NULL,
                current_rms REAL NOT NULL,
                active_power REAL NOT NULL,
                reactive_power REAL NOT NULL,
                apparent_power REAL NOT NULL,
                power_factor REAL NOT NULL,
                frequency REAL NOT NULL,
                FOREIGN KEY (meter_id) REFERENCES meters(meter_id) ON DELETE CASCADE
            )
        """)
        
        # Check if power_factor column exists, add it if not (for existing databases)
        cursor = await self.conn.execute("PRAGMA table_info(readings)")
        columns = await cursor.fetchall()
        column_names = [col[1] for col in columns]

        if 'power_factor' not in column_names:
            logger.info("Adding power_factor column to existing readings table")
            await self.conn.execute("ALTER TABLE readings ADD COLUMN power_factor REAL DEFAULT 0.0")
            await self.conn.commit()

        # Check if energy_poll_interval column exists in meters table, add it if not
        cursor = await self.conn.execute("PRAGMA table_info(meters)")
        columns = await cursor.fetchall()
        column_names = [col[1] for col in columns]

        if 'energy_poll_interval' not in column_names:
            logger.info("Adding energy_poll_interval column to existing meters table")
            await self.conn.execute("ALTER TABLE meters ADD COLUMN energy_poll_interval REAL DEFAULT 30.0")
            await self.conn.commit()
        
        # Create indexes for efficient queries
        await self.conn.execute("""
            CREATE INDEX IF NOT EXISTS idx_readings_meter_time 
            ON readings(meter_id, timestamp DESC)
        """)
        
        await self.conn.execute("""
            CREATE INDEX IF NOT EXISTS idx_readings_timestamp 
            ON readings(timestamp DESC)
        """)
        
        await self.conn.execute("""
            CREATE TABLE IF NOT EXISTS settings (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        """)

        # Create energy readings table
        await self.conn.execute("""
            CREATE TABLE IF NOT EXISTS energy_readings (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TIMESTAMP NOT NULL,
                meter_id TEXT NOT NULL,
                phase TEXT NOT NULL,
                total_kwh REAL NOT NULL,
                FOREIGN KEY (meter_id) REFERENCES meters(meter_id) ON DELETE CASCADE
            )
        """)

        # Create index for efficient energy queries
        await self.conn.execute("""
            CREATE INDEX IF NOT EXISTS idx_energy_readings_meter_phase_time
            ON energy_readings(meter_id, phase, timestamp DESC)
        """)

        await self.conn.commit()
        logger.info(f"Database initialized at {self.db_path}")
    
    async def close(self):
        """Close database connection"""
        if self.conn:
            await self.conn.close()
    
    # Meter Management
    async def register_meter(self, meter: Meter):
        """Register or update a meter"""
        await self.conn.execute("""
            INSERT INTO meters (meter_id, ip_address, name, location, enabled, poll_interval, energy_poll_interval, last_seen)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(meter_id) DO UPDATE SET
                ip_address=excluded.ip_address,
                last_seen=excluded.last_seen
        """, (
            meter.meter_id,
            meter.ip_address,
            meter.name,
            meter.location,
            1 if meter.enabled else 0,
            meter.poll_interval,
            meter.energy_poll_interval,
            datetime.utcnow()
        ))
        await self.conn.commit()
    
    async def get_all_meters(self) -> List[Dict]:
        """Get all registered meters"""
        cursor = await self.conn.execute("""
            SELECT meter_id, ip_address, name, location, enabled, poll_interval, energy_poll_interval, last_seen
            FROM meters
            ORDER BY name, meter_id
        """)
        rows = await cursor.fetchall()
        return [dict(row) for row in rows]
    
    async def get_meter(self, meter_id: str) -> Optional[Dict]:
        """Get a specific meter"""
        cursor = await self.conn.execute("""
            SELECT meter_id, ip_address, name, location, enabled, poll_interval, energy_poll_interval, last_seen
            FROM meters
            WHERE meter_id = ?
        """, (meter_id,))
        row = await cursor.fetchone()
        return dict(row) if row else None
    
    async def update_meter(self, meter_id: str, meter: Meter):
        """Update meter configuration"""
        await self.conn.execute("""
            UPDATE meters
            SET ip_address=?, name=?, location=?, enabled=?, poll_interval=?, energy_poll_interval=?
            WHERE meter_id=?
        """, (
            meter.ip_address,
            meter.name,
            meter.location,
            1 if meter.enabled else 0,
            meter.poll_interval,
            meter.energy_poll_interval,
            meter_id
        ))
        await self.conn.commit()
    
    async def delete_meter(self, meter_id: str):
        """Delete a meter and all its readings"""
        await self.conn.execute("DELETE FROM meters WHERE meter_id=?", (meter_id,))
        await self.conn.commit()
    
    # Reading Storage
    async def store_reading(self, reading: MeterReading):
        """Store a meter reading"""
        await self.conn.execute("""
            INSERT INTO readings (
                timestamp, meter_id, voltage_rms, current_rms,
                active_power, reactive_power, apparent_power, power_factor, frequency
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            reading.timestamp,
            reading.meter_id,
            reading.voltage_rms,
            reading.current_rms,
            reading.active_power,
            reading.reactive_power,
            reading.apparent_power,
            reading.power_factor,
            reading.frequency
        ))
        await self.conn.commit()
    
    async def get_latest_reading(self, meter_id: str) -> Optional[Dict]:
        """Get the most recent reading for a meter"""
        cursor = await self.conn.execute("""
            SELECT timestamp, meter_id, voltage_rms, current_rms,
                   active_power, reactive_power, apparent_power, power_factor, frequency
            FROM readings
            WHERE meter_id = ?
            ORDER BY timestamp DESC
            LIMIT 1
        """, (meter_id,))
        row = await cursor.fetchone()
        return dict(row) if row else None
    
    async def get_reading_count(
        self,
        meter_id: str,
        start_time: datetime,
        end_time: datetime
    ) -> int:
        """Get count of readings in a time range without fetching the data"""
        cursor = await self.conn.execute("""
            SELECT COUNT(*) as count
            FROM readings
            WHERE meter_id = ? AND timestamp BETWEEN ? AND ?
        """, (meter_id, start_time, end_time))
        row = await cursor.fetchone()
        return row['count'] if row else 0

    def _calculate_optimal_aggregation(self, count: int, time_span_seconds: float, target_points: int = 10000) -> str:
        """Calculate optimal aggregation level to target a specific number of points"""
        if count <= target_points:
            return "none"

        # Calculate optimal interval in minutes
        time_span_minutes = time_span_seconds / 60
        optimal_minutes = time_span_minutes / target_points

        # Round to nice bucket sizes
        nice_buckets_minutes = [1, 2, 3, 5, 10, 15, 20, 30, 60, 120, 180, 360, 720, 1440, 10080, 43200]
        nice_bucket_names = ["1min", "2min", "3min", "5min", "10min", "15min", "20min", "30min",
                             "1hour", "2hour", "3hour", "6hour", "12hour", "1day", "1week", "1month"]

        # Find the smallest nice bucket that's >= optimal
        for i, minutes in enumerate(nice_buckets_minutes):
            if minutes >= optimal_minutes:
                return nice_bucket_names[i]

        # If we need more than a month, use month
        return "1month"

    def _parse_aggregation_to_sql(self, aggregation: str) -> tuple:
        """
        Parse aggregation string and return (sql_grouping_expression, interval_seconds)
        Supports: 1min, 5min, Nmin, 1hour, 1day, 1week, 1month
        """
        if aggregation == "none":
            return None, None

        # Handle minute intervals (1min, 5min, 10min, etc.)
        if aggregation.endswith("min"):
            minutes = int(aggregation[:-3])
            # Group by N-minute intervals using Unix timestamp math
            sql_expr = f"datetime((strftime('%s', timestamp) / {minutes * 60}) * {minutes * 60}, 'unixepoch')"
            return sql_expr, minutes * 60

        # Handle hour intervals
        if aggregation.endswith("hour"):
            hours = int(aggregation[:-4])
            if hours == 1:
                sql_expr = "strftime('%Y-%m-%d %H:00:00', timestamp)"
            else:
                sql_expr = f"datetime((strftime('%s', timestamp) / {hours * 3600}) * {hours * 3600}, 'unixepoch')"
            return sql_expr, hours * 3600

        # Handle day intervals
        if aggregation.endswith("day"):
            days = int(aggregation[:-3])
            if days == 1:
                sql_expr = "strftime('%Y-%m-%d 00:00:00', timestamp)"
            else:
                sql_expr = f"datetime((strftime('%s', timestamp) / {days * 86400}) * {days * 86400}, 'unixepoch')"
            return sql_expr, days * 86400

        # Handle week intervals (start Sunday)
        if aggregation == "1week":
            # SQLite strftime('%w', date) returns 0 for Sunday, 1 for Monday, etc.
            # Round down to the most recent Sunday
            sql_expr = "datetime(strftime('%s', timestamp) - (strftime('%w', timestamp) * 86400), 'unixepoch', 'start of day')"
            return sql_expr, 7 * 86400

        # Handle month intervals (calendar months)
        if aggregation == "1month":
            sql_expr = "strftime('%Y-%m-01 00:00:00', timestamp)"
            return sql_expr, 30 * 86400  # Approximate for display purposes

        # Default to none
        return None, None
    
    async def get_historical_readings(
        self,
        meter_id: str,
        start_time: datetime,
        end_time: datetime,
        limit: Optional[int] = 10000,
        aggregation: str = "auto"
    ) -> Dict:
        """Get historical readings with optional aggregation

        Returns dict with:
          - readings: list of reading dicts
          - aggregation_applied: str indicating what aggregation was used
          - original_count: int (only if auto was used)
        """
        result = {
            "readings": [],
            "aggregation_applied": aggregation,
            "query_time_seconds": 0
        }

        import time
        query_start = time.time()

        # Handle auto aggregation
        if aggregation == "auto":
            count = await self.get_reading_count(meter_id, start_time, end_time)
            time_span_seconds = (end_time - start_time).total_seconds()
            aggregation = self._calculate_optimal_aggregation(count, time_span_seconds, target_points=10000)
            result["aggregation_applied"] = aggregation
            result["original_count"] = count
            logger.info(f"Auto-aggregation: {count} points -> {aggregation}")

        # Handle no aggregation (raw data)
        if aggregation == "none":
            query = """
                SELECT timestamp, meter_id, voltage_rms, current_rms,
                       active_power, reactive_power, apparent_power, power_factor, frequency
                FROM readings
                WHERE meter_id = ? AND timestamp BETWEEN ? AND ?
                ORDER BY timestamp DESC
            """
            params = [meter_id, start_time, end_time]
            if limit:
                query += " LIMIT ?"
                params.append(limit)

            cursor = await self.conn.execute(query, params)
        else:
            # Parse aggregation and build query
            sql_grouping, interval_seconds = self._parse_aggregation_to_sql(aggregation)

            if sql_grouping is None:
                # Fallback to raw data if parsing failed
                query = """
                    SELECT timestamp, meter_id, voltage_rms, current_rms,
                           active_power, reactive_power, apparent_power, power_factor, frequency
                    FROM readings
                    WHERE meter_id = ? AND timestamp BETWEEN ? AND ?
                    ORDER BY timestamp DESC
                """
                params = [meter_id, start_time, end_time]
                if limit:
                    query += " LIMIT ?"
                    params.append(limit)
            else:
                # Aggregate by time period
                query = f"""
                    SELECT
                        {sql_grouping} as timestamp,
                        meter_id,
                        AVG(voltage_rms) as voltage_rms,
                        AVG(current_rms) as current_rms,
                        AVG(active_power) as active_power,
                        AVG(reactive_power) as reactive_power,
                        AVG(apparent_power) as apparent_power,
                        AVG(power_factor) as power_factor,
                        AVG(frequency) as frequency
                    FROM readings
                    WHERE meter_id = ? AND timestamp BETWEEN ? AND ?
                    GROUP BY {sql_grouping}
                    ORDER BY timestamp DESC
                """
                params = [meter_id, start_time, end_time]
                if limit:
                    query += " LIMIT ?"
                    params.append(limit)

            cursor = await self.conn.execute(query, params)

        rows = await cursor.fetchall()
        result["readings"] = [dict(row) for row in rows]
        result["query_time_seconds"] = round(time.time() - query_start, 3)

        return result
    
    async def get_statistics(
        self,
        meter_id: str,
        start_time: datetime,
        end_time: datetime
    ) -> Dict:
        """Calculate statistics for a time period"""
        cursor = await self.conn.execute("""
            SELECT 
                COUNT(*) as sample_count,
                AVG(voltage_rms) as avg_voltage,
                MAX(voltage_rms) as max_voltage,
                MIN(voltage_rms) as min_voltage,
                AVG(current_rms) as avg_current,
                MAX(current_rms) as max_current,
                AVG(active_power) as avg_power,
                MAX(active_power) as max_power,
                SUM(active_power) / 3600000.0 as total_energy_kwh
            FROM readings
            WHERE meter_id = ? AND timestamp BETWEEN ? AND ?
        """, (meter_id, start_time, end_time))
        
        row = await cursor.fetchone()
        if row:
            stats = dict(row)
            stats['meter_id'] = meter_id
            stats['start_time'] = start_time
            stats['end_time'] = end_time
            return stats
        return None
    
    # Settings Management
    async def get_settings(self) -> Dict:
        """Get all system settings"""
        cursor = await self.conn.execute("SELECT key, value FROM settings")
        rows = await cursor.fetchall()
        settings = {row['key']: json.loads(row['value']) for row in rows}

        # Return defaults if not set
        return {
            'poll_interval': settings.get('poll_interval', 1.0),
            'energy_poll_interval': settings.get('energy_poll_interval', 30.0),
            'data_retention_days': settings.get('data_retention_days', None),
            'max_websocket_clients': settings.get('max_websocket_clients', 100)
        }
    
    async def update_settings(self, settings: SystemSettings):
        """Update system settings"""
        for key, value in settings.dict(exclude_none=True).items():
            await self.conn.execute("""
                INSERT INTO settings (key, value, updated_at)
                VALUES (?, ?, ?)
                ON CONFLICT(key) DO UPDATE SET
                    value=excluded.value,
                    updated_at=excluded.updated_at
            """, (key, json.dumps(value), datetime.utcnow()))
        await self.conn.commit()
    
    # Data Maintenance
    async def cleanup_old_data(self, days: int):
        """Delete data older than specified days"""
        cutoff_date = datetime.utcnow() - timedelta(days=days)
        cursor = await self.conn.execute("""
            DELETE FROM readings WHERE timestamp < ?
        """, (cutoff_date,))
        await self.conn.commit()
        deleted = cursor.rowcount
        logger.info(f"Cleaned up {deleted} old readings")
        return deleted
        
    async def get_database_stats(self):
        """Get database statistics including disk space"""
        import os
        import shutil
        
        try:
            # Database file sizes
            db_size = os.path.getsize(self.db_path) / (1024 * 1024)  # MB
            wal_path = f"{self.db_path}-wal"
            wal_size = os.path.getsize(wal_path) / (1024 * 1024) if os.path.exists(wal_path) else 0
            
            # Reading counts
            cursor = await self.conn.execute("SELECT COUNT(*) as count FROM readings")
            row = await cursor.fetchone()
            reading_count = row['count'] if row else 0
            
            # Oldest and newest readings
            cursor = await self.conn.execute("""
                SELECT 
                    MIN(timestamp) as oldest_reading,
                    MAX(timestamp) as newest_reading
                FROM readings
            """)
            row = await cursor.fetchone()
            oldest = row['oldest_reading'] if row and row['oldest_reading'] else None
            newest = row['newest_reading'] if row and row['newest_reading'] else None
            
            # Disk space on the partition where database is stored
            stat = shutil.disk_usage(os.path.dirname(self.db_path))
            disk_total = stat.total / (1024 * 1024 * 1024)  # GB
            disk_used = stat.used / (1024 * 1024 * 1024)
            disk_free = stat.free / (1024 * 1024 * 1024)
            disk_percent = (stat.used / stat.total) * 100
            
            return {
                'db_size_mb': round(db_size, 2),
                'wal_size_mb': round(wal_size, 2),
                'total_size_mb': round(db_size + wal_size, 2),
                'reading_count': reading_count,
                'oldest_reading': oldest,
                'newest_reading': newest,
                'disk_total_gb': round(disk_total, 2),
                'disk_used_gb': round(disk_used, 2),
                'disk_free_gb': round(disk_free, 2),
                'disk_percent_used': round(disk_percent, 2)
            }
        except Exception as e:
            logger.error(f"Error getting database stats: {e}")
            return None
    
    async def aggregate_old_data(self, days_old: int = 90, aggregation_hours: int = 1):
        """
        Aggregate very old data to save space while preserving trends.
        Replaces high-frequency old data with hourly averages.
        
        Args:
            days_old: Only aggregate data older than this many days
            aggregation_hours: Aggregate into this many hour buckets (default 1 = hourly)
        """
        try:
            cutoff_date = datetime.utcnow() - timedelta(days=days_old)
            
            # First, create aggregated records
            logger.info(f"Aggregating data older than {days_old} days into {aggregation_hours}-hour averages...")
            
            await self.conn.execute(f"""
                INSERT INTO readings (
                    timestamp, meter_id, voltage_rms, current_rms,
                    active_power, reactive_power, apparent_power, power_factor, frequency
                )
                SELECT 
                    datetime(strftime('%Y-%m-%d %H:00:00', timestamp), 
                            '+' || (CAST(strftime('%H', timestamp) AS INTEGER) / {aggregation_hours} * {aggregation_hours}) || ' hours') as timestamp,
                    meter_id,
                    AVG(voltage_rms) as voltage_rms,
                    AVG(current_rms) as current_rms,
                    AVG(active_power) as active_power,
                    AVG(reactive_power) as reactive_power,
                    AVG(apparent_power) as apparent_power,
                    AVG(power_factor) as power_factor,
                    AVG(frequency) as frequency
                FROM readings
                WHERE timestamp < ?
                GROUP BY 
                    meter_id,
                    strftime('%Y-%m-%d', timestamp),
                    CAST(strftime('%H', timestamp) AS INTEGER) / {aggregation_hours}
                HAVING COUNT(*) > 1
            """, (cutoff_date,))
            
            # Then delete the original high-frequency records, keeping the aggregated ones
            cursor = await self.conn.execute("""
                DELETE FROM readings
                WHERE timestamp < ?
                AND id NOT IN (
                    SELECT MAX(id)
                    FROM readings
                    WHERE timestamp < ?
                    GROUP BY 
                        meter_id,
                        strftime('%Y-%m-%d %H:00:00', timestamp)
                )
            """, (cutoff_date, cutoff_date))
            
            await self.conn.commit()
            deleted = cursor.rowcount
            logger.info(f"Aggregated and removed {deleted} old high-frequency readings")
            return deleted
            
        except Exception as e:
            logger.error(f"Error aggregating old data: {e}")
            return 0
    
    async def cleanup_oldest_data(self, target_free_space_gb: float = 1.0, batch_days: int = 30):
        """
        Delete oldest data in batches until target free space is achieved.
        
        Args:
            target_free_space_gb: Stop deleting when this much free space is available
            batch_days: Delete this many days at a time
        """
        import shutil
        import os
        
        try:
            total_deleted = 0
            
            while True:
                # Check current disk space
                stat = shutil.disk_usage(os.path.dirname(self.db_path))
                disk_free_gb = stat.free / (1024 * 1024 * 1024)
                
                if disk_free_gb >= target_free_space_gb:
                    logger.info(f"Target free space achieved: {disk_free_gb:.2f}GB")
                    break
                
                # Find oldest data
                cursor = await self.conn.execute("""
                    SELECT MIN(timestamp) as oldest FROM readings
                """)
                row = await cursor.fetchone()
                
                if not row or not row['oldest']:
                    logger.warning("No more data to delete!")
                    break
                
                oldest = datetime.fromisoformat(row['oldest'].replace('Z', '+00:00'))
                cutoff = oldest + timedelta(days=batch_days)
                
                # Delete oldest batch
                cursor = await self.conn.execute("""
                    DELETE FROM readings WHERE timestamp < ?
                """, (cutoff,))
                
                await self.conn.commit()
                deleted = cursor.rowcount
                total_deleted += deleted
                
                logger.info(f"Deleted {deleted} readings older than {cutoff}. Total deleted: {total_deleted}")
                
                # Checkpoint to reclaim space
                await self.checkpoint_wal()
                await self.conn.execute("VACUUM")
                
                if deleted == 0:
                    break
            
            return total_deleted
            
        except Exception as e:
            logger.error(f"Error cleaning up oldest data: {e}")
            return 0
    
    async def checkpoint_wal(self):
        """Checkpoint WAL to prevent it from growing too large"""
        try:
            await self.conn.execute("PRAGMA wal_checkpoint(TRUNCATE)")
            await self.conn.commit()
            logger.debug("WAL checkpoint completed")
        except Exception as e:
            logger.error(f"Error during WAL checkpoint: {e}")

    # Energy Reading Management
    async def store_energy_reading(self, reading):
        """Store an energy reading (timestamp generated by Pi)"""
        from models import EnergyReading
        await self.conn.execute("""
            INSERT INTO energy_readings (
                timestamp, meter_id, phase, total_kwh
            ) VALUES (?, ?, ?, ?)
        """, (
            reading.timestamp,
            reading.meter_id,
            reading.phase,
            reading.total_kwh
        ))
        await self.conn.commit()

    async def get_latest_energy_reading(self, meter_id: str, phase: Optional[str] = None) -> Optional[Dict]:
        """Get the most recent energy reading for a meter and phase"""
        if phase and phase != 'ALL':
            cursor = await self.conn.execute("""
                SELECT timestamp, meter_id, phase, total_kwh
                FROM energy_readings
                WHERE meter_id = ? AND phase = ?
                ORDER BY timestamp DESC
                LIMIT 1
            """, (meter_id, phase))
        else:
            cursor = await self.conn.execute("""
                SELECT timestamp, meter_id, phase, total_kwh
                FROM energy_readings
                WHERE meter_id = ?
                ORDER BY timestamp DESC
                LIMIT 1
            """, (meter_id,))
        row = await cursor.fetchone()
        return dict(row) if row else None

    async def get_energy_readings(
        self,
        meter_id: str,
        start_time: datetime,
        end_time: datetime,
        phase: Optional[str] = None
    ) -> List[Dict]:
        """Get energy readings for a time range, optionally filtered by phase"""
        if phase and phase != 'ALL':
            cursor = await self.conn.execute("""
                SELECT timestamp, meter_id, phase, total_kwh
                FROM energy_readings
                WHERE meter_id = ? AND phase = ? AND timestamp BETWEEN ? AND ?
                ORDER BY timestamp ASC
            """, (meter_id, phase, start_time, end_time))
        else:
            cursor = await self.conn.execute("""
                SELECT timestamp, meter_id, phase, total_kwh
                FROM energy_readings
                WHERE meter_id = ? AND timestamp BETWEEN ? AND ?
                ORDER BY timestamp ASC
            """, (meter_id, start_time, end_time))

        rows = await cursor.fetchall()
        return [dict(row) for row in rows]
    def _interpolate_kwh_at_time(self, readings_with_ms: List[Dict], target_time_ms: int) -> Optional[float]:
        """
        Linear interpolation using binary search - O(log n) instead of O(n)
        Assumes readings_with_ms is already sorted by time_ms
        """
        if not readings_with_ms or len(readings_with_ms) == 0:
            return None

        # Binary search for the position where target would be inserted
        import bisect

        # Extract just the timestamps for binary search
        times = [r['time_ms'] for r in readings_with_ms]

        # Find insertion point
        idx = bisect.bisect_left(times, target_time_ms)

        # Check for exact match
        if idx < len(readings_with_ms) and times[idx] == target_time_ms:
            return readings_with_ms[idx]['total_kwh']

        # Handle edge cases
        if idx == 0:
            # Target is before all readings
            return readings_with_ms[0]['total_kwh']
        if idx >= len(readings_with_ms):
            # Target is after all readings
            return readings_with_ms[-1]['total_kwh']

        # Interpolate between readings at idx-1 and idx
        before = readings_with_ms[idx - 1]
        after = readings_with_ms[idx]

        time_diff = after['time_ms'] - before['time_ms']
        kwh_diff = after['total_kwh'] - before['total_kwh']

        if time_diff == 0:
            return before['total_kwh']

        ratio = (target_time_ms - before['time_ms']) / time_diff
        return before['total_kwh'] + (kwh_diff * ratio)

    def _calculate_energy_deltas_without_interpolation(
        self,
        energy_readings: List[Dict],
        aggregation: str,
        start_time: datetime,
        end_time: datetime
    ) -> List[Dict]:
        """
        Calculate energy deltas without interpolation (Option A)
        Find first and last actual reading within each time bucket
        Used for hour+ aggregations where interpolation is not needed
        """
        if not energy_readings or len(energy_readings) < 2:
            return []

        # Convert readings to milliseconds for efficient searching
        readings_with_ms = []
        for r in energy_readings:
            ts_str = r['timestamp']
            if not ts_str.endswith('Z'):
                ts_str += 'Z'
            dt = datetime.fromisoformat(ts_str.replace('Z', '+00:00'))
            readings_with_ms.append({
                **r,
                'time_ms': int(dt.timestamp() * 1000)
            })

        # Ensure sorted by time
        readings_with_ms.sort(key=lambda x: x['time_ms'])

        # Get bucket boundaries
        buckets = []
        current = start_time.replace(second=0, microsecond=0)

        if aggregation.endswith("hour"):
            hours = int(aggregation[:-4])
            current = current.replace(minute=0)
            current = current.replace(hour=(current.hour // hours) * hours)

            while current <= end_time:
                bucket_start = current
                bucket_end = current + timedelta(hours=hours)
                buckets.append({
                    'start_ms': int(bucket_start.timestamp() * 1000),
                    'end_ms': int(bucket_end.timestamp() * 1000),
                    'bucket_start': bucket_start  # Keep datetime for formatting
                })
                current = bucket_end

        elif aggregation.endswith("day"):
            days = int(aggregation[:-3])
            current = current.replace(hour=0, minute=0)

            while current <= end_time:
                bucket_start = current
                bucket_end = current + timedelta(days=days)
                buckets.append({
                    'start_ms': int(bucket_start.timestamp() * 1000),
                    'end_ms': int(bucket_end.timestamp() * 1000),
                    'bucket_start': bucket_start
                })
                current = bucket_end

        elif aggregation == "1week":
            current = current.replace(hour=0, minute=0)
            days_since_sunday = current.weekday() + 1 if current.weekday() != 6 else 0
            current = current - timedelta(days=days_since_sunday)

            while current <= end_time:
                bucket_start = current
                bucket_end = current + timedelta(days=7)
                buckets.append({
                    'start_ms': int(bucket_start.timestamp() * 1000),
                    'end_ms': int(bucket_end.timestamp() * 1000),
                    'bucket_start': bucket_start
                })
                current = bucket_end

        elif aggregation == "1month":
            current = current.replace(day=1, hour=0, minute=0)

            while current <= end_time:
                bucket_start = current
                if current.month == 12:
                    bucket_end = current.replace(year=current.year + 1, month=1)
                else:
                    bucket_end = current.replace(month=current.month + 1)

                buckets.append({
                    'start_ms': int(bucket_start.timestamp() * 1000),
                    'end_ms': int(bucket_end.timestamp() * 1000),
                    'bucket_start': bucket_start
                })
                current = bucket_end

        # Calculate delta for each bucket using first/last readings
        result = []
        for bucket in buckets:
            # Find first and last reading within this bucket
            first_reading = None
            last_reading = None

            for reading in readings_with_ms:
                if bucket['start_ms'] <= reading['time_ms'] < bucket['end_ms']:
                    if first_reading is None:
                        first_reading = reading
                    last_reading = reading

            # Calculate delta if we have both readings
            if first_reading and last_reading:
                delta = last_reading['total_kwh'] - first_reading['total_kwh']

                if delta >= 0:
                    result.append({
                        'timestamp': bucket['bucket_start'].isoformat(),
                        'time': bucket['bucket_start'].strftime(self._get_time_format(aggregation)),
                        'energy_kwh': delta
                    })
                else:
                    logger.warning(f"Negative energy delta detected: {delta:.6f} kWh (ignored)")

        return result

    def _get_time_format(self, aggregation: str) -> str:
        """Get appropriate time format string for aggregation level"""
        if aggregation.endswith('hour'):
            return '%m/%d %I:00 %p'
        elif aggregation.endswith('day'):
            return '%m/%d/%Y'
        elif aggregation == '1week':
            return '%m/%d/%Y'
        elif aggregation == '1month':
            return '%m/%Y'
        else:
            return '%m/%d %I:%M %p'

    def _calculate_energy_deltas_with_interpolation(
        self,
        energy_readings: List[Dict],
        aggregation: str,
        start_time: datetime,
        end_time: datetime
    ) -> List[Dict]:
        """
        Calculate energy deltas using linear interpolation at bucket boundaries
        Optimized to O(n log m) using binary search instead of O(n×m)
        Only used for minute-level aggregations (1min, 5min, etc.)
        """
        if not energy_readings or len(energy_readings) < 2:
            return []

        # Pre-process readings ONCE - convert to milliseconds and sort
        readings_with_ms = []
        for r in energy_readings:
            ts_str = r['timestamp']
            if not ts_str.endswith('Z'):
                ts_str += 'Z'
            dt = datetime.fromisoformat(ts_str.replace('Z', '+00:00'))
            readings_with_ms.append({
                **r,
                'time_ms': int(dt.timestamp() * 1000)
            })

        # Ensure sorted by time (should already be, but make sure)
        readings_with_ms.sort(key=lambda x: x['time_ms'])

        # Get bucket boundaries
        buckets = []
        current = start_time.replace(second=0, microsecond=0)

        if aggregation.endswith("min"):
            minutes = int(aggregation[:-3])
            # Round down to nearest N-minute mark
            current = current.replace(minute=(current.minute // minutes) * minutes)

            while current <= end_time:
                bucket_start = current
                bucket_end = current + timedelta(minutes=minutes)
                buckets.append({
                    'start_ms': int(bucket_start.timestamp() * 1000),
                    'end_ms': int(bucket_end.timestamp() * 1000),
                    'bucket_start': bucket_start,
                    'display_time': bucket_start.strftime('%m/%d %I:%M %p')
                })
                current = bucket_end

        elif aggregation.endswith("hour"):
            hours = int(aggregation[:-4])
            current = current.replace(minute=0)
            current = current.replace(hour=(current.hour // hours) * hours)

            while current <= end_time:
                bucket_start = current
                bucket_end = current + timedelta(hours=hours)
                buckets.append({
                    'start_ms': int(bucket_start.timestamp() * 1000),
                    'end_ms': int(bucket_end.timestamp() * 1000),
                    'display_time': bucket_start.strftime('%m/%d %I:00 %p')
                })
                current = bucket_end

        elif aggregation.endswith("day"):
            days = int(aggregation[:-3])
            current = current.replace(hour=0, minute=0)

            while current <= end_time:
                bucket_start = current
                bucket_end = current + timedelta(days=days)
                buckets.append({
                    'start_ms': int(bucket_start.timestamp() * 1000),
                    'end_ms': int(bucket_end.timestamp() * 1000),
                    'display_time': bucket_start.strftime('%m/%d/%Y')
                })
                current = bucket_end

        elif aggregation == "1week":
            # Round down to most recent Sunday
            current = current.replace(hour=0, minute=0)
            days_since_sunday = current.weekday() + 1 if current.weekday() != 6 else 0
            current = current - timedelta(days=days_since_sunday)

            while current <= end_time:
                bucket_start = current
                bucket_end = current + timedelta(days=7)
                buckets.append({
                    'start_ms': int(bucket_start.timestamp() * 1000),
                    'end_ms': int(bucket_end.timestamp() * 1000),
                    'display_time': bucket_start.strftime('%m/%d/%Y')
                })
                current = bucket_end

        elif aggregation == "1month":
            # Round down to first of month
            current = current.replace(day=1, hour=0, minute=0)

            while current <= end_time:
                bucket_start = current
                # Calculate next month
                if current.month == 12:
                    bucket_end = current.replace(year=current.year + 1, month=1)
                else:
                    bucket_end = current.replace(month=current.month + 1)

                buckets.append({
                    'start_ms': int(bucket_start.timestamp() * 1000),
                    'end_ms': int(bucket_end.timestamp() * 1000),
                    'display_time': bucket_start.strftime('%m/%Y')
                })
                current = bucket_end

        # Calculate delta for each bucket using interpolation with preprocessed readings
        result = []
        for bucket in buckets:
            start_kwh = self._interpolate_kwh_at_time(readings_with_ms, bucket['start_ms'])
            end_kwh = self._interpolate_kwh_at_time(readings_with_ms, bucket['end_ms'])

            if start_kwh is not None and end_kwh is not None:
                delta = end_kwh - start_kwh

                # Ignore negative deltas (meter reset or error)
                if delta >= 0:
                    result.append({
                        'timestamp': bucket['bucket_start'].isoformat(),
                        'time': bucket['display_time'],
                        'energy_kwh': delta
                    })
                else:
                    logger.warning(f"Negative energy delta detected: {delta:.6f} kWh (ignored)")

        return result

    async def get_energy_aggregated(
        self,
        meter_id: str,
        start_time: datetime,
        end_time: datetime,
        phase: str,
        aggregation: str = "auto"
    ) -> Dict:
        """Get energy data with interpolation and aggregation

        Returns dict with:
          - aggregated: list of energy deltas per bucket
          - raw_total_kwh: total energy from first to last reading
          - raw_readings_count: number of raw readings
          - aggregation_applied: what aggregation was used
        """
        import time
        query_start = time.time()

        result = {
            "aggregated": [],
            "raw_total_kwh": 0,
            "raw_readings_count": 0,
            "aggregation_applied": aggregation
        }

        # Get raw readings
        raw_readings = await self.get_energy_readings(meter_id, start_time, end_time, phase)
        result["raw_readings_count"] = len(raw_readings)

        # Calculate raw total (first to last)
        if len(raw_readings) >= 2:
            result["raw_total_kwh"] = raw_readings[-1]['total_kwh'] - raw_readings[0]['total_kwh']

        # Handle auto aggregation for energy data
        if aggregation == "auto":
            count = len(raw_readings)
            time_span_seconds = (end_time - start_time).total_seconds()
            aggregation = self._calculate_optimal_aggregation(count, time_span_seconds, target_points=10000)
            result["aggregation_applied"] = aggregation
            logger.info(f"Energy auto-aggregation: {count} points -> {aggregation}")

        # Calculate aggregated deltas (if not 'none')
        if aggregation != "none" and len(raw_readings) >= 2:
            # Use interpolation only for minute-level aggregations (1min, 5min, etc.)
            # For hour+ aggregations, use simpler first/last reading method
            if aggregation.endswith('min'):
                logger.info(f"Using interpolation for {aggregation} aggregation")
                result["aggregated"] = self._calculate_energy_deltas_with_interpolation(
                    raw_readings, aggregation, start_time, end_time
                )
            else:
                logger.info(f"Skipping interpolation for {aggregation} aggregation (using first/last readings)")
                result["aggregated"] = self._calculate_energy_deltas_without_interpolation(
                    raw_readings, aggregation, start_time, end_time
                )

        result["query_time_seconds"] = round(time.time() - query_start, 3)

        return result
