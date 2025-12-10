from pydantic import BaseModel, Field
from typing import Optional, List
from datetime import datetime
from enum import Enum

class Meter(BaseModel):
    meter_id: str = Field(..., description="Unique meter identifier")
    ip_address: str = Field(..., description="IP address of the meter")
    name: Optional[str] = Field(None, description="Friendly name")
    location: Optional[str] = Field(None, description="Physical location")
    enabled: bool = Field(True, description="Whether polling is enabled")
    poll_interval: float = Field(1.0, description="Polling interval in seconds")
    energy_poll_interval: float = Field(30.0, description="Energy polling interval in seconds")

    class Config:
        json_schema_extra = {
            "example": {
                "meter_id": "meter_001",
                "ip_address": "192.168.1.100",
                "name": "Main Panel",
                "location": "Garage",
                "enabled": True,
                "poll_interval": 1.0,
                "energy_poll_interval": 30.0
            }
        }

class MeterReading(BaseModel):
    timestamp: datetime
    meter_id: str
    voltage_rms: float = Field(..., description="RMS Voltage (V)")
    current_rms: float = Field(..., description="RMS Current (A)")
    active_power: float = Field(..., description="Active Power (W)")
    reactive_power: float = Field(..., description="Reactive Power (VAR)")
    apparent_power: float = Field(..., description="Apparent Power (VA)")
    power_factor: float = Field(..., description="Power Factor")
    frequency: float = Field(..., description="Frequency (Hz)")

class EnergyReading(BaseModel):
    timestamp: datetime
    meter_id: str
    phase: str = Field(..., description="Phase (A, B, or C)")
    total_kwh: float = Field(..., description="Accumulated energy in kWh")

class MeterConfig(BaseModel):
    name: Optional[str] = None
    location: Optional[str] = None
    enabled: Optional[bool] = None
    poll_interval: Optional[float] = None
    energy_poll_interval: Optional[float] = None

class AggregationType(str, Enum):
    NONE = "none"
    AUTO = "auto"
    MINUTE_1 = "1min"
    MINUTE_5 = "5min"
    HOUR_1 = "1hour"
    DAY_1 = "1day"
    WEEK_1 = "1week"
    MONTH_1 = "1month"

class HistoricalQuery(BaseModel):
    start_time: datetime
    end_time: datetime
    limit: Optional[int] = Field(10000, description="Maximum number of records")
    aggregation: Optional[str] = Field("auto", description="Aggregation level: auto, none, 1min, 5min, 1hour, 1day, 1week, 1month, or Nmin for arbitrary minutes")

class SystemSettings(BaseModel):
    poll_interval: Optional[float] = Field(1.0, description="Default polling interval")
    energy_poll_interval: Optional[float] = Field(30.0, description="Default energy polling interval")
    data_retention_days: Optional[int] = Field(None, description="Auto-delete data older than N days (None = keep forever)")
    min_free_space_gb: Optional[float] = Field(1.0, description="Minimum free disk space in GB before auto-cleanup")
    max_websocket_clients: Optional[int] = Field(100)
    
class MeterStatistics(BaseModel):
    meter_id: str
    start_time: datetime
    end_time: datetime
    avg_voltage: float
    max_voltage: float
    min_voltage: float
    avg_current: float
    max_current: float
    avg_power: float
    max_power: float
    total_energy_kwh: float
    sample_count: int

class EnergyCalibrationStart(BaseModel):
    phases: str = Field(..., description="Phases to calibrate (A, B, C, AB, AC, BC, ABC)")

class EnergyCalibrationComplete(BaseModel):
    phase: str = Field(..., description="Phase that was calibrated (A, B, or C)")
    loadWatts: float = Field(..., description="Known load in watts")
    durationMinutes: float = Field(..., description="Duration the load ran in minutes")

class EnergyHistoricalQuery(BaseModel):
    start_time: datetime
    end_time: datetime
    phase: Optional[str] = Field("ALL", description="Phase filter (A, B, C, or ALL)")
    aggregation: Optional[str] = Field("auto", description="Aggregation level: auto, none, 1min, 5min, 1hour, 1day, 1week, 1month")