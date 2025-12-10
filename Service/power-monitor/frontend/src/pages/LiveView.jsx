import React, { useState, useEffect, useRef, useCallback, useMemo } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from 'recharts';
import { Zap, TrendingUp, Activity, Radio } from 'lucide-react';

const LiveView = ({ meters }) => {
  const [selectedMeter, setSelectedMeter] = useState(null);
  const [liveData, setLiveData] = useState([]);
  const [currentReading, setCurrentReading] = useState(null);
  const [timeWindow, setTimeWindow] = useState(300); // Time window in seconds (default 5 minutes)
  const [refreshRate, setRefreshRate] = useState(1000); // Refresh every 1000ms (1 second)
  const [currentTime, setCurrentTime] = useState(Date.now()); // For smooth scrolling
  const isFetchingRef = useRef(false); // Prevent overlapping requests
  const pendingDataRef = useRef(null); // Store new data points to sync with animation

  // No buffer - only keep visible data

  // Continuous time updates for smooth scrolling (~15 FPS) using requestAnimationFrame
  // Throttled to reduce memory pressure and unnecessary re-renders
  // Also processes pending data updates to batch them with time updates
  useEffect(() => {
    let animationId;
    let frameCount = 0;
    const FRAME_SKIP = 4; // Update every 4 frames (~15 FPS instead of 60 FPS)

    const animate = () => {
      frameCount++;

      // Only update every Nth frame to reduce render frequency
      if (frameCount >= FRAME_SKIP) {
        frameCount = 0;

        // Batch time update with any pending data update
        const now = Date.now();

        if (pendingDataRef.current) {
          // We have new data - update both time and data in a single state update
          const newPoint = pendingDataRef.current;
          pendingDataRef.current = null;

          setLiveData(prev => {
            const timestampMs = newPoint.timestamp;

            // Find the correct position to insert based on timestamp
            const insertIndex = prev.findIndex(point => point.timestamp > timestampMs);

            if (insertIndex === -1) {
              // New data is newest, append to end
              return [...prev, newPoint];
            } else if (prev[insertIndex].timestamp === timestampMs) {
              // Replace existing data at this exact timestamp
              const updated = [...prev];
              updated[insertIndex] = newPoint;
              return updated;
            } else {
              // Insert at the correct position
              const updated = [...prev];
              updated.splice(insertIndex, 0, newPoint);
              return updated;
            }
          });
        }

        setCurrentTime(now);
      }

      animationId = requestAnimationFrame(animate);
    };

    animationId = requestAnimationFrame(animate);
    return () => cancelAnimationFrame(animationId);
  }, []);

  // Only reinitialize data when meter changes
  useEffect(() => {
    if (selectedMeter) {
      setLiveData([]); // Start fresh for new meter
    }
  }, [selectedMeter]);

  // Cleanup old data - only keep data within visible window
  useEffect(() => {
    const cleanup = () => {
      const now = Date.now();
      const windowMs = timeWindow * 1000;
      const cutoffTime = now - windowMs; // Delete anything outside visible window

      setLiveData(prev => {
        const filtered = prev.filter(point => point.timestamp >= cutoffTime);
        // Return filtered data only if changed (prevent unnecessary re-renders)
        return filtered.length < prev.length ? filtered : prev;
      });
    };

    // Run cleanup every second
    const intervalId = setInterval(cleanup, 1000);
    return () => clearInterval(intervalId);
  }, [timeWindow]);

  useEffect(() => {
    if (meters.length > 0 && !selectedMeter) {
      setSelectedMeter(meters[0].meter_id);
    }
  }, [meters, selectedMeter]);

  useEffect(() => {
    if (!selectedMeter) return;

    // Fetch function to avoid overlap
    const fetchLiveData = async () => {
      // Skip if already fetching
      if (isFetchingRef.current) return;

      isFetchingRef.current = true;

      try {
        const response = await fetch(`/api/meters/${selectedMeter}/live-direct`);
        const data = await response.json();

        setCurrentReading(data);

        // Store new data point in ref - it will be added on the next animation frame
        // This synchronizes data updates with the animation loop to prevent lag/snapping
        const timestamp = new Date(data.timestamp + 'Z'); // Add 'Z' to parse as UTC
        const timestampMs = timestamp.getTime();

        pendingDataRef.current = {
          timestamp: timestampMs,
          time: timestamp.toLocaleTimeString('en-US', { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' }),
          fullTime: timestamp.toLocaleString(),
          power: data.active_power,
          voltage: data.voltage_rms,
          current: data.current_rms
        };
      } catch (error) {
        console.error('Error fetching live data:', error);
      } finally {
        isFetchingRef.current = false;
      }
    };

    // Initial fetch
    fetchLiveData();

    // Set up interval
    const interval = setInterval(fetchLiveData, refreshRate);

    return () => clearInterval(interval);
  }, [selectedMeter, refreshRate]);

  const getSelectedMeterInfo = () => {
    return meters.find(m => m.meter_id === selectedMeter);
  };

  const meterInfo = getSelectedMeterInfo();

  // Calculate chart domain for smooth scrolling
  const windowMs = timeWindow * 1000;
  const chartDomain = [currentTime - windowMs, currentTime];

  // Memoize filtered data to prevent recalculation on every render
  const chartData = useMemo(() => {
    return liveData.filter(point =>
      point.timestamp >= chartDomain[0] && point.timestamp <= chartDomain[1]
    );
  }, [liveData, chartDomain[0], chartDomain[1]]);

  // Memoize ticks generation to prevent recalculation on every render
  const ticks = useMemo(() => {
    const tickInterval = timeWindow <= 60 ? 5000 : // 5 seconds for 1-minute window
                        timeWindow <= 300 ? 30000 : // 30 seconds for 5-minute window
                        timeWindow <= 600 ? 60000 : // 1 minute for 10-minute window
                        timeWindow <= 1800 ? 300000 : // 5 minutes for 30-minute window
                        600000; // 10 minutes for 1-hour window

    const ticksArray = [];
    const startTime = chartDomain[0];
    const endTime = chartDomain[1];

    // Start from the first tick that's a multiple of tickInterval after startTime
    const firstTick = Math.ceil(startTime / tickInterval) * tickInterval;

    for (let tick = firstTick; tick <= endTime; tick += tickInterval) {
      ticksArray.push(tick);
    }

    return ticksArray;
  }, [timeWindow, chartDomain[0], chartDomain[1]]);

  // Format timestamp for X-axis display (memoized to prevent recreation)
  const formatXAxis = useCallback((timestamp) => {
    const date = new Date(timestamp);
    return date.toLocaleTimeString('en-US', {
      hour12: false,
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    });
  }, []);

  // Memoize data point count to prevent recalculation
  const dataPointsWithValues = useMemo(() => {
    return chartData.filter(d => d.power !== null).length;
  }, [chartData]);

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex flex-col md:flex-row md:items-center justify-between gap-4">
        <h1 className="text-3xl font-bold text-gray-900 dark:text-white">Live Monitoring</h1>
        <div className="flex items-center space-x-2">
          <Radio className="h-5 w-5 text-green-500 animate-pulse" />
          <span className="text-sm text-gray-600 dark:text-gray-400">Live</span>
        </div>
      </div>

      {/* Controls */}
      <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-4">
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
          {/* Meter Selection */}
          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
              Select Meter
            </label>
            <select
              value={selectedMeter || ''}
              onChange={(e) => setSelectedMeter(e.target.value)}
              className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
            >
              {meters.map(meter => (
                <option key={meter.meter_id} value={meter.meter_id}>
                  {meter.name || meter.meter_id} - {meter.location || meter.ip_address}
                </option>
              ))}
            </select>
          </div>

          {/* Refresh Rate */}
          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
              Refresh Rate
            </label>
            <select
              value={refreshRate}
              onChange={(e) => setRefreshRate(Number(e.target.value))}
              className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
            >
              <option value={100}>10 Hz (100ms)</option>
              <option value={200}>5 Hz (200ms)</option>
              <option value={500}>2 Hz (500ms)</option>
              <option value={1000}>1 Hz (1s)</option>
              <option value={2000}>0.5 Hz (2s)</option>
              <option value={5000}>0.2 Hz (5s)</option>
            </select>
          </div>

          {/* Time Window */}
          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
              Time Window
            </label>
            <select
              value={timeWindow}
              onChange={(e) => setTimeWindow(Number(e.target.value))}
              className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
            >
              <option value={60}>1 minute</option>
              <option value={300}>5 minutes</option>
              <option value={600}>10 minutes</option>
              <option value={1800}>30 minutes</option>
              <option value={3600}>1 hour</option>
            </select>
            <p className="text-xs text-gray-500 dark:text-gray-400 mt-1">
              {dataPointsWithValues} data points in view
            </p>
          </div>
        </div>
      </div>

      {/* Current Readings Cards */}
      {currentReading && currentReading.active_power !== undefined && (
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
          <StatCard
            icon={Zap}
            title="Active Power"
            value={currentReading.active_power.toFixed(2)}
            unit="W"
            color="blue"
          />
          <StatCard
            icon={TrendingUp}
            title="Voltage"
            value={currentReading.voltage_rms.toFixed(2)}
            unit="V"
            color="green"
          />
          <StatCard
            icon={Activity}
            title="Current"
            value={currentReading.current_rms.toFixed(3)}
            unit="A"
            color="purple"
          />
          <StatCard
            icon={Radio}
            title="Frequency"
            value={currentReading.frequency.toFixed(2)}
            unit="Hz"
            color="orange"
          />
        </div>
      )}

      {/* Charts */}
      <div className="grid grid-cols-1 gap-6">
        {/* Power Chart - Full Width */}
        <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
          <h3 className="text-lg font-semibold text-gray-900 dark:text-white mb-4">
            Power (W)
          </h3>
          <ResponsiveContainer width="100%" height={400}>
            <LineChart data={chartData} syncId="liveMonitor">
              <CartesianGrid strokeDasharray="3 3" stroke="#374151" />
              <XAxis
                type="number"
                dataKey="timestamp"
                domain={chartDomain}
                ticks={ticks}
                tickFormatter={formatXAxis}
                stroke="#9CA3AF"
                tick={{ fill: '#9CA3AF', fontSize: 12 }}
                scale="time"
              />
              <YAxis
                stroke="#9CA3AF"
                tick={{ fill: '#9CA3AF' }}
              />
              <Tooltip
                contentStyle={{
                  backgroundColor: '#1F2937',
                  border: '1px solid #374151',
                  borderRadius: '0.5rem',
                  color: '#F3F4F6'
                }}
                labelFormatter={formatXAxis}
                animationDuration={0}
                isAnimationActive={false}
              />
              <Legend wrapperStyle={{ color: '#9CA3AF' }} />
              <Line
                type="monotone"
                dataKey="power"
                stroke="#3B82F6"
                strokeWidth={2}
                dot={false}
                name="Power (W)"
                isAnimationActive={false}
                connectNulls={false}
              />
            </LineChart>
          </ResponsiveContainer>
        </div>

        {/* Voltage and Current Charts Side by Side */}
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          {/* Voltage Chart */}
          <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
            <h3 className="text-lg font-semibold text-gray-900 dark:text-white mb-4">
              Voltage (V)
            </h3>
            <ResponsiveContainer width="100%" height={300}>
              <LineChart data={chartData} syncId="liveMonitor">
                <CartesianGrid strokeDasharray="3 3" stroke="#374151" />
                <XAxis
                  type="number"
                  dataKey="timestamp"
                  domain={chartDomain}
                  ticks={ticks}
                  tickFormatter={formatXAxis}
                  stroke="#9CA3AF"
                  tick={{ fill: '#9CA3AF' }}
                  scale="time"
                />
                <YAxis
                  stroke="#9CA3AF"
                  tick={{ fill: '#9CA3AF' }}
                />
                <Tooltip
                  contentStyle={{
                    backgroundColor: '#1F2937',
                    border: '1px solid #374151',
                    borderRadius: '0.5rem',
                    color: '#F3F4F6'
                  }}
                  labelFormatter={formatXAxis}
                  animationDuration={0}
                />
                <Legend wrapperStyle={{ color: '#9CA3AF' }} />
                <Line
                  type="monotone"
                  dataKey="voltage"
                  stroke="#10B981"
                  strokeWidth={2}
                  dot={false}
                  name="Voltage (V)"
                  isAnimationActive={false}
                  connectNulls={false}
                />
              </LineChart>
            </ResponsiveContainer>
          </div>

          {/* Current Chart */}
          <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
            <h3 className="text-lg font-semibold text-gray-900 dark:text-white mb-4">
              Current (A)
            </h3>
            <ResponsiveContainer width="100%" height={300}>
              <LineChart data={chartData} syncId="liveMonitor">
                <CartesianGrid strokeDasharray="3 3" stroke="#374151" />
                <XAxis
                  type="number"
                  dataKey="timestamp"
                  domain={chartDomain}
                  ticks={ticks}
                  tickFormatter={formatXAxis}
                  stroke="#9CA3AF"
                  tick={{ fill: '#9CA3AF' }}
                  scale="time"
                />
                <YAxis
                  stroke="#9CA3AF"
                  tick={{ fill: '#9CA3AF' }}
                />
                <Tooltip
                  contentStyle={{
                    backgroundColor: '#1F2937',
                    border: '1px solid #374151',
                    borderRadius: '0.5rem',
                    color: '#F3F4F6'
                  }}
                  labelFormatter={formatXAxis}
                  animationDuration={0}
                />
                <Legend wrapperStyle={{ color: '#9CA3AF' }} />
                <Line
                  type="monotone"
                  dataKey="current"
                  stroke="#8B5CF6"
                  strokeWidth={2}
                  dot={false}
                  name="Current (A)"
                  isAnimationActive={false}
                  connectNulls={false}
                />
              </LineChart>
            </ResponsiveContainer>
          </div>
        </div>
      </div>

      {/* Additional Info */}
      {currentReading && currentReading.reactive_power !== undefined && (
        <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
          <h3 className="text-lg font-semibold text-gray-900 dark:text-white mb-4">Additional Parameters</h3>
          <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
            <div>
              <p className="text-sm text-gray-600 dark:text-gray-400">Reactive Power</p>
              <p className="text-2xl font-bold text-gray-900 dark:text-white">
                {currentReading.reactive_power.toFixed(2)} <span className="text-sm font-normal">VAR</span>
              </p>
            </div>
            <div>
              <p className="text-sm text-gray-600 dark:text-gray-400">Apparent Power</p>
              <p className="text-2xl font-bold text-gray-900 dark:text-white">
                {currentReading.apparent_power.toFixed(2)} <span className="text-sm font-normal">VA</span>
              </p>
            </div>
            <div>
              <p className="text-sm text-gray-600 dark:text-gray-400">Power Factor</p>
              <p className="text-2xl font-bold text-gray-900 dark:text-white">
                {currentReading.apparent_power > 0 
                  ? (currentReading.active_power / currentReading.apparent_power).toFixed(3)
                  : '0.000'}
              </p>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

const StatCard = ({ icon: Icon, title, value, unit, color }) => {
  const colorClasses = {
    blue: 'bg-blue-100 dark:bg-blue-900 text-blue-600 dark:text-blue-300',
    green: 'bg-green-100 dark:bg-green-900 text-green-600 dark:text-green-300',
    purple: 'bg-purple-100 dark:bg-purple-900 text-purple-600 dark:text-purple-300',
    orange: 'bg-orange-100 dark:bg-orange-900 text-orange-600 dark:text-orange-300'
  };

  return (
    <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
      <div className="flex items-center justify-between">
        <div>
          <p className="text-sm text-gray-600 dark:text-gray-400 mb-1">{title}</p>
          <p className="text-3xl font-bold text-gray-900 dark:text-white">
            {value}
            <span className="text-lg font-normal text-gray-600 dark:text-gray-400 ml-2">{unit}</span>
          </p>
        </div>
        <div className={`p-3 rounded-lg ${colorClasses[color]}`}>
          <Icon className="h-8 w-8" />
        </div>
      </div>
    </div>
  );
};

export default LiveView;