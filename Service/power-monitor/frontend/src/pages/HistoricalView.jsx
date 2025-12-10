import React, { useState, useEffect } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, BarChart, Bar, Brush } from 'recharts';
import { Download, Calendar, TrendingUp, DollarSign, AlertCircle, X } from 'lucide-react';
import { format, subDays, subHours, subMonths } from 'date-fns';

const HistoricalView = ({ meters }) => {
  const [selectedMeters, setSelectedMeters] = useState([]);
  const [historicalData, setHistoricalData] = useState({});
  const [energyData, setEnergyData] = useState({});
  const [loading, setLoading] = useState(false);
  const [dateRange, setDateRange] = useState('24h');
  const [startDate, setStartDate] = useState('');
  const [endDate, setEndDate] = useState('');
  const [aggregation, setAggregation] = useState('auto');
  const [selectedMetrics, setSelectedMetrics] = useState(['active_power']);
  const [showEnergy, setShowEnergy] = useState(false);
  const [costPerKWh, setCostPerKWh] = useState(0.12);
  const [resampleInfo, setResampleInfo] = useState(null);
  const [performanceInfo, setPerformanceInfo] = useState(null);
  const [showLargeDatasetWarning, setShowLargeDatasetWarning] = useState(false);
  const [largeDatasetInfo, setLargeDatasetInfo] = useState(null);
  const [energyTotals, setEnergyTotals] = useState({});
  const [appliedAggregation, setAppliedAggregation] = useState(null);
  const [energyLoading, setEnergyLoading] = useState(false);
  const [lastPowerFetchTime, setLastPowerFetchTime] = useState(null);
  const [lastFetchParams, setLastFetchParams] = useState(null);
  const [energyRestricted, setEnergyRestricted] = useState(false);
  const [energyRestrictionMessage, setEnergyRestrictionMessage] = useState('');
  const [showEnergyWarning, setShowEnergyWarning] = useState(true);

  useEffect(() => {
    if (meters.length > 0 && selectedMeters.length === 0) {
      setSelectedMeters([meters[0].meter_id]);
    }
  }, [meters, selectedMeters]);

  // Check energy data restrictions based on timespan and aggregation
  useEffect(() => {
    const { start, end } = getDateRange();
    const timespanHours = (end - start) / (1000 * 60 * 60);

    // Skip restriction check for 'auto' and 'none' aggregations
    if (aggregation === 'auto' || aggregation === 'none') {
      setEnergyRestricted(false);
      setEnergyRestrictionMessage('');
      setShowEnergyWarning(true);
      return;
    }

    // Check restrictions for manual aggregation levels
    let restricted = false;
    let message = '';

    if (aggregation === '1min' && timespanHours > 25) {
      restricted = true;
      message = 'The aggregation level (1 minute) is too fine for the length of time selected (>25 hours). Please choose a coarser aggregation or shorter timespan.';
    } else if (aggregation === '5min' && timespanHours > 100) {
      restricted = true;
      message = 'The aggregation level (5 minutes) is too fine for the length of time selected (>100 hours). Please choose a coarser aggregation or shorter timespan.';
    }

    setEnergyRestricted(restricted);
    setEnergyRestrictionMessage(message);
    setShowEnergyWarning(true); // Reset warning visibility when restrictions change
  }, [dateRange, aggregation, startDate, endDate]);

  const getDateRange = () => {
    const end = new Date();
    let start;

    switch (dateRange) {
      case '1h':
        start = new Date(end.getTime() - (1 * 60 * 60 * 1000));
        break;
	  case '12h':
        start = new Date(end.getTime() - (12 * 60 * 60 * 1000));
        break;
      case '24h':
        start = new Date(end.getTime() - (24 * 60 * 60 * 1000));
        break;
      case '7d':
        start = new Date(end.getTime() - (7 * 24 * 60 * 60 * 1000));
        break;
      case '30d':
        start = new Date(end.getTime() - (30 * 24 * 60 * 60 * 1000));
        break;
      case '90d':
        start = new Date(end.getTime() - (90 * 24 * 60 * 60 * 1000));
        break;
      case 'custom':
        if (startDate && endDate) {
          return {
            start: new Date(startDate),
            end: new Date(endDate)
          };
        }
        return { 
          start: new Date(end.getTime() - (24 * 60 * 60 * 1000)), 
          end 
        };
      default:
        start = new Date(end.getTime() - (24 * 60 * 60 * 1000));
    }

    return { start, end };
  };

  const average = (values) => {
    const valid = values.filter(v => v != null && !isNaN(v));
    return valid.length > 0 ? valid.reduce((a, b) => a + b, 0) / valid.length : null;
  };

  const showResampleNotification = (info) => {
    setResampleInfo(info);
    console.log(`Auto-resampled: ${info.originalPoints} points -> ${info.finalPoints} points (${info.aggregationType})`);
    setTimeout(() => setResampleInfo(null), 10000);
  };

  const insertGapsForMissingData = (data, expectedIntervalMs, gapThreshold = 2.5) => {
    if (!data || data.length < 2) return data;

    const result = [];
    const maxAllowedGap = expectedIntervalMs * gapThreshold;

    console.log(`[Gap Detection] Expected interval: ${(expectedIntervalMs/1000).toFixed(2)}s, threshold: ${(maxAllowedGap/1000).toFixed(2)}s`);

    for (let i = 0; i < data.length; i++) {
      result.push(data[i]);

      if (i < data.length - 1) {
        const currentTime = data[i].timestamp;
        const nextTime = data[i + 1].timestamp;
        const actualGap = nextTime - currentTime;

        // If gap is larger than threshold, insert null points to show the gap
        if (actualGap > maxAllowedGap) {
          console.log(`[Gap Detected] ${(actualGap/1000).toFixed(1)}s gap between ${new Date(currentTime).toLocaleTimeString()} and ${new Date(nextTime).toLocaleTimeString()}`);
          
          // Insert null points at regular intervals within the gap
          const numNullPoints = Math.min(Math.ceil(actualGap / expectedIntervalMs) - 1, 100);
          console.log(`  Inserting ${numNullPoints} null points`);
          
          for (let j = 1; j <= numNullPoints; j++) {
            const gapTime = currentTime + (j * actualGap / (numNullPoints + 1));
            const gapDate = new Date(gapTime);
            result.push({
              time: format(gapDate, 'MM/dd hh:mm a'),
              fullTime: gapDate.toLocaleString(),
              timestamp: gapTime,
              active_power: null,
              voltage_rms: null,
              current_rms: null,
              frequency: null,
              reactive_power: null,
              apparent_power: null,
              power_factor: null
            });
          }
        }
      }
    }

    const gapsInserted = result.length - data.length;
    if (gapsInserted > 0) {
      console.log(`[Gap Detection] Inserted ${gapsInserted} null points into ${data.length} original points`);
    }
    return result;
  };

  const calculateOptimalAggregation = (count, timeSpanSeconds, targetPoints = 10000) => {
    if (count <= targetPoints) {
      return "none";
    }

    // Calculate optimal interval in minutes
    const timeSpanMinutes = timeSpanSeconds / 60;
    const optimalMinutes = timeSpanMinutes / targetPoints;

    // Nice bucket sizes in minutes
    const niceBucketsMinutes = [1, 2, 3, 5, 10, 15, 20, 30, 60, 120, 180, 360, 720, 1440, 10080, 43200];
    const niceBucketNames = ["1min", "2min", "3min", "5min", "10min", "15min", "20min", "30min",
                             "1hour", "2hour", "3hour", "6hour", "12hour", "1day", "1week", "1month"];

    // Find the smallest nice bucket that's >= optimal
    for (let i = 0; i < niceBucketsMinutes.length; i++) {
      if (niceBucketsMinutes[i] >= optimalMinutes) {
        return niceBucketNames[i];
      }
    }

    // If we need more than a month, use month
    return "1month";
  };

  const fetchHistoricalData = async (skipWarning = false) => {
    if (selectedMeters.length === 0) return;

    const { start, end } = getDateRange();

    // Check for large dataset warning if aggregation is 'none'
    if (aggregation === 'none' && !skipWarning) {
      const countResponse = await fetch(`/api/meters/${selectedMeters[0]}/count`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          start_time: start.toISOString(),
          end_time: end.toISOString()
        })
      });
      const countData = await countResponse.json();
      const count = countData.count;

      if (count > 10000) {
        // Show warning modal
        const sizeEstimateMB = (count * 0.2 / 1000).toFixed(1); // Rough estimate
        const timeEstimate = Math.ceil(count / 5000); // Rough seconds estimate
        setLargeDatasetInfo({
          count,
          sizeEstimateMB,
          timeEstimate
        });
        setShowLargeDatasetWarning(true);
        return; // Don't fetch yet, wait for user confirmation
      }
    }

    const overallStart = performance.now();
    console.log('\n========== STARTING DATA FETCH ==========');

    setLoading(true);
    try {
      console.log(`Time range: ${start.toISOString()} to ${end.toISOString()}`);

      // For auto aggregation with multiple meters, calculate unified aggregation level
      let unifiedAggregation = aggregation;
      let totalOriginalCount = 0;

      if (aggregation === 'auto' && selectedMeters.length > 1) {
        console.log('[Auto Aggregation] Multiple meters detected, calculating unified aggregation...');

        // Get counts for all meters
        for (const meterId of selectedMeters) {
          try {
            const countResponse = await fetch(`/api/meters/${meterId}/count`, {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({
                start_time: start.toISOString(),
                end_time: end.toISOString()
              })
            });
            const countData = await countResponse.json();
            const count = countData.count;
            console.log(`[Auto Aggregation] Meter ${meterId}: ${count} points`);
            totalOriginalCount += count;
          } catch (error) {
            console.error(`Error getting count for meter ${meterId}:`, error);
          }
        }

        // Calculate optimal aggregation based on total count
        const timeSpanSeconds = (end - start) / 1000;
        unifiedAggregation = calculateOptimalAggregation(totalOriginalCount, timeSpanSeconds);
        console.log(`[Auto Aggregation] Total count: ${totalOriginalCount}, Unified aggregation: ${unifiedAggregation}`);
      }

      const allData = {};
      let autoAggregationInfo = null;
      let totalFinalCount = 0;

      for (const meterId of selectedMeters) {
        const meterStart = performance.now();
        console.log(`\n[Meter: ${meterId}] Starting fetch`);

        // Get meter configuration for poll interval (for gap detection)
        const meterConfig = meters.find(m => m.meter_id === meterId);
        const meterPollInterval = meterConfig?.poll_interval || 1.0;

        // Fetch power data (backend aggregates)
        const powerResponse = await fetch(`/api/meters/${meterId}/historical`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            start_time: start.toISOString(),
            end_time: end.toISOString(),
            limit: null,
            aggregation: unifiedAggregation
          })
        });

        const powerData = await powerResponse.json();
        console.log(`[Power] ${powerData.count} readings, aggregation: ${unifiedAggregation} -> ${powerData.aggregation_applied}`);
        console.log(`Backend query time: ${(powerData.query_time_seconds * 1000).toFixed(0)}ms`);

        // Track counts for auto-aggregation notification
        if (aggregation === 'auto') {
          if (selectedMeters.length === 1) {
            // Single meter: use backend's original_count
            if (powerData.original_count && !autoAggregationInfo) {
              autoAggregationInfo = {
                applied: true,
                aggregationType: powerData.aggregation_applied,
                originalPoints: powerData.original_count,
                finalPoints: powerData.count
              };
            }
          } else {
            // Multiple meters: track totals
            totalFinalCount += powerData.count;
            if (!autoAggregationInfo) {
              autoAggregationInfo = {
                applied: true,
                aggregationType: powerData.aggregation_applied,
                originalPoints: totalOriginalCount,
                finalPoints: 0  // Will be updated after loop
              };
            }
          }
        }

        // Transform and prepare power data for charts
        let transformed = powerData.readings.map(reading => {
          const timestamp = new Date(reading.timestamp + 'Z');
          return {
            time: format(timestamp, 'MM/dd hh:mm a'),
            fullTime: timestamp.toLocaleString(),
            timestamp: timestamp.getTime(),
            active_power: reading.active_power,
            voltage_rms: reading.voltage_rms,
            current_rms: reading.current_rms,
            frequency: reading.frequency,
            reactive_power: reading.reactive_power,
            apparent_power: reading.apparent_power,
            power_factor: reading.power_factor
          };
        }).reverse(); // Backend returns DESC, we want ASC for time series

        // Apply gap detection (on frontend since data is already small)
        let expectedInterval;
        const aggApplied = powerData.aggregation_applied;
        if (aggApplied.endsWith('min')) {
          const minutes = parseInt(aggApplied.slice(0, -3));
          expectedInterval = minutes * 60 * 1000;
        } else if (aggApplied.endsWith('hour')) {
          const hours = parseInt(aggApplied.slice(0, -4));
          expectedInterval = hours * 60 * 60 * 1000;
        } else if (aggApplied.endsWith('day')) {
          const days = parseInt(aggApplied.slice(0, -3));
          expectedInterval = days * 24 * 60 * 60 * 1000;
        } else if (aggApplied === '1week') {
          expectedInterval = 7 * 24 * 60 * 60 * 1000;
        } else if (aggApplied === '1month') {
          expectedInterval = 30 * 24 * 60 * 60 * 1000;
        } else {
          expectedInterval = meterPollInterval * 1000;
        }

        transformed = insertGapsForMissingData(transformed, expectedInterval);
        allData[meterId] = transformed;

        const meterTime = performance.now() - meterStart;
        console.log(`Total meter time: ${meterTime.toFixed(0)}ms`);
      }

      setHistoricalData(allData);

      // Update final count for multiple meters and show auto-aggregation notification
      if (autoAggregationInfo) {
        if (selectedMeters.length > 1 && aggregation === 'auto') {
          autoAggregationInfo.finalPoints = totalFinalCount;
        }
        showResampleNotification(autoAggregationInfo);
      }

      const overallTime = performance.now() - overallStart;
      console.log(`\n[Complete] Total fetch time: ${overallTime.toFixed(0)}ms\n`);

      // Track last fetch time and parameters
      setLastPowerFetchTime(Date.now());
      setLastFetchParams({
        start: start.toISOString(),
        end: end.toISOString(),
        aggregation: aggregation,
        selectedMeters: [...selectedMeters],
        showEnergy: showEnergy,
        appliedAggregation: autoAggregationInfo ? autoAggregationInfo.aggregationType : aggregation
      });

      setPerformanceInfo({
        totalTime: overallTime.toFixed(0),
        pointsLoaded: Object.values(allData).reduce((sum, data) => sum + data.length, 0),
        startTime: start,
        endTime: end,
        aggregation: autoAggregationInfo ? autoAggregationInfo.aggregationType : aggregation
      });

    } catch (error) {
      console.error('[Error] Failed to fetch historical data:', error);
    } finally {
      setLoading(false);
    }
  };

  const fetchEnergyData = async () => {
    if (selectedMeters.length === 0) return;
    if (!showEnergy) return;
    if (energyRestricted) return;

    const { start, end } = getDateRange();
    const timespanHours = (end - start) / (1000 * 60 * 60);

    setEnergyLoading(true);
    try {
      console.log('\n========== STARTING ENERGY FETCH ==========');

      const allEnergyData = {};
      const allEnergyTotals = {};

      // Determine energy aggregation level
      let energyAggregation = aggregation;

      // Apply auto aggregation rules for energy (different from power)
      if (aggregation === 'auto') {
        if (timespanHours < 25) {
          energyAggregation = '1min';
        } else if (timespanHours < 100) {
          energyAggregation = '5min';
        } else {
          energyAggregation = '1hour';
        }
        console.log(`[Energy Auto] Timespan: ${timespanHours.toFixed(1)}h -> Using ${energyAggregation} aggregation`);
      }

      for (const meterId of selectedMeters) {
        try {
          const energyResponse = await fetch(`/api/meters/${meterId}/energy/historical`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
              start_time: start.toISOString(),
              end_time: end.toISOString(),
              phase: 'A',
              aggregation: energyAggregation
            })
          });
          const energyResult = await energyResponse.json();

          // Use backend-calculated aggregated data and raw total
          allEnergyData[meterId] = energyResult.aggregated || [];
          allEnergyTotals[meterId] = energyResult.raw_total_kwh || 0;

          // Store the actual applied aggregation for tooltip formatting
          if (energyResult.aggregation_applied) {
            setAppliedAggregation(energyResult.aggregation_applied);
          }

          console.log(`[Energy] ${energyResult.aggregated.length} buckets, raw total: ${energyResult.raw_total_kwh.toFixed(3)} kWh`);
        } catch (error) {
          console.error(`Error fetching energy data for ${meterId}:`, error);
          allEnergyData[meterId] = [];
          allEnergyTotals[meterId] = 0;
        }
      }

      setEnergyData(allEnergyData);
      setEnergyTotals(allEnergyTotals);

      console.log('[Energy] Fetch complete\n');
    } catch (error) {
      console.error('[Error] Failed to fetch energy data:', error);
    } finally {
      setEnergyLoading(false);
    }
  };

  const handleRefresh = async () => {
    if (selectedMeters.length === 0) return;

    const { start, end } = getDateRange();
    const now = Date.now();
    const timeSinceLastFetch = lastPowerFetchTime ? (now - lastPowerFetchTime) / 1000 : null;

    // Build current parameters for comparison
    const currentParams = {
      start: start.toISOString(),
      end: end.toISOString(),
      aggregation: aggregation,
      selectedMeters: [...selectedMeters],
      showEnergy: showEnergy
    };

    // Check if any parameter (except showEnergy) has changed
    const paramsChanged = !lastFetchParams ||
      currentParams.start !== lastFetchParams.start ||
      currentParams.end !== lastFetchParams.end ||
      currentParams.aggregation !== lastFetchParams.aggregation ||
      JSON.stringify(currentParams.selectedMeters) !== JSON.stringify(lastFetchParams.selectedMeters);

    // Check if ONLY showEnergy changed
    const onlyEnergyCheckboxChanged = !paramsChanged &&
      lastFetchParams &&
      currentParams.showEnergy !== lastFetchParams.showEnergy;

    // Determine what to fetch
    const moreThan10Seconds = !timeSinceLastFetch || timeSinceLastFetch > 10;

    console.log('[Refresh Logic]', {
      timeSinceLastFetch,
      moreThan10Seconds,
      paramsChanged,
      onlyEnergyCheckboxChanged
    });

    // Apply refresh rules:
    // 1. If no changes AND <10s: Skip fetch entirely
    if (!paramsChanged && !moreThan10Seconds && !onlyEnergyCheckboxChanged) {
      console.log('[Refresh] Skipping fetch - no changes and <10s since last fetch');
      return;
    }

    // 2. Exception - Only energy checkbox changed AND <10s: Fetch only energy
    if (onlyEnergyCheckboxChanged && !moreThan10Seconds) {
      console.log('[Refresh] Fetching only energy data (checkbox changed, <10s)');
      if (showEnergy && !energyRestricted) {
        await fetchEnergyData();
        // Update lastFetchParams to reflect energy checkbox change
        setLastFetchParams({
          ...lastFetchParams,
          showEnergy: showEnergy
        });
      }
      return;
    }

    // 3. Changes made OR >10s passed: Fetch everything
    console.log('[Refresh] Fetching all data (changes detected or >10s)');
    await fetchHistoricalData();

    if (showEnergy && !energyRestricted) {
      await fetchEnergyData();
    }
  };

  const exportData = async () => {
    if (selectedMeters.length === 0) return;

    try {
      const { start, end } = getDateRange();

      if (selectedMeters.length === 1) {
        const meterId = selectedMeters[0];

        // Download power data CSV
        const powerUrl = `/api/meters/${meterId}/export?start_time=${start.toISOString()}&end_time=${end.toISOString()}`;
        window.open(powerUrl, '_blank');

        // Wait before downloading energy data to avoid browser blocking
        await new Promise(resolve => setTimeout(resolve, 300));

        // Download energy data CSV
        const energyUrl = `/api/meters/${meterId}/energy/export?start_time=${start.toISOString()}&end_time=${end.toISOString()}&phase=A`;
        window.open(energyUrl, '_blank');
      } else {
        // Multiple meters - download all files with delays
        for (const meterId of selectedMeters) {
          // Download power data CSV
          const powerUrl = `/api/meters/${meterId}/export?start_time=${start.toISOString()}&end_time=${end.toISOString()}`;
          window.open(powerUrl, '_blank');
          await new Promise(resolve => setTimeout(resolve, 300));

          // Download energy data CSV
          const energyUrl = `/api/meters/${meterId}/energy/export?start_time=${start.toISOString()}&end_time=${end.toISOString()}&phase=A`;
          window.open(energyUrl, '_blank');
          await new Promise(resolve => setTimeout(resolve, 300));
        }
      }
    } catch (error) {
      console.error('Error exporting data:', error);
    }
  };

  const metricConfig = {
    active_power: { name: 'Active Power (W)', color: '#3B82F6', yAxisId: 'power' },
    voltage_rms: { name: 'Voltage (V)', color: '#10B981', yAxisId: 'voltage' },
    current_rms: { name: 'Current (A)', color: '#8B5CF6', yAxisId: 'current' },
    frequency: { name: 'Frequency (Hz)', color: '#F59E0B', yAxisId: 'frequency' },
    reactive_power: { name: 'Reactive Power (VAR)', color: '#EF4444', yAxisId: 'power' },
    apparent_power: { name: 'Apparent Power (VA)', color: '#EC4899', yAxisId: 'power' },
    power_factor: { name: 'Power Factor', color: '#14B8A6', yAxisId: 'pf' }
  };

  const lineColors = [
    ['#3B82F6', '#10B981', '#8B5CF6', '#F59E0B', '#EF4444', '#EC4899', '#14B8A6'],
    ['#60A5FA', '#34D399', '#A78BFA', '#FBBF24', '#F87171', '#F472B6', '#2DD4BF'],
    ['#93C5FD', '#6EE7B7', '#C4B5FD', '#FCD34D', '#FCA5A5', '#F9A8D4', '#5EEAD4'],
  ];

  const barColors = [
    ['#3B82F6', '#10B981'],
    ['#8B5CF6', '#F59E0B'],
    ['#EF4444', '#EC4899'],
  ];

  const getLineColor = (meterIdx, metric) => {
    const metricIdx = Object.keys(metricConfig).indexOf(metric);
    return lineColors[meterIdx % lineColors.length][metricIdx % 7];
  };

  const getBarColor = (meterIdx, barIdx) => {
    return barColors[meterIdx % barColors.length][barIdx];
  };

  const toggleMeter = (meterId) => {
    setSelectedMeters(prev =>
      prev.includes(meterId) ? prev.filter(id => id !== meterId) : [...prev, meterId]
    );
  };

  const toggleMetric = (metric) => {
    setSelectedMetrics(prev =>
      prev.includes(metric) ? prev.filter(m => m !== metric) : [...prev, metric]
    );
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold text-gray-900 dark:text-white">Historical Data</h1>
          {performanceInfo ? (
            <p className="text-sm text-gray-600 dark:text-gray-400 mt-1">
              Loaded {performanceInfo.pointsLoaded.toLocaleString()} points in {performanceInfo.totalTime}ms from {format(performanceInfo.startTime, 'MM/dd hh:mm a')} to {format(performanceInfo.endTime, 'MM/dd hh:mm a')} with {performanceInfo.aggregation} aggregation
            </p>
          ) : (
            <p className="text-sm text-gray-500 dark:text-gray-500 mt-1">
              Click Refresh to load data
            </p>
          )}
        </div>
        <button
          onClick={exportData}
          disabled={selectedMeters.length === 0 || loading}
          className="inline-flex items-center px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg shadow transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
        >
          <Download className="h-5 w-5 mr-2" />
          Export CSV
        </button>
      </div>

      {/* Controls */}
      <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
          {/* Meter Selection */}
          <div className="md:col-span-2">
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
              Select Meters
            </label>
            <div className="flex flex-wrap gap-2">
              {meters.map(meter => (
                <button
                  key={meter.meter_id}
                  onClick={() => toggleMeter(meter.meter_id)}
                  className={`px-4 py-2 rounded-lg font-medium transition-colors ${
                    selectedMeters.includes(meter.meter_id)
                      ? 'bg-blue-600 text-white'
                      : 'bg-gray-200 dark:bg-gray-700 text-gray-700 dark:text-gray-300'
                  }`}
                >
                  {meter.name || meter.meter_id}
                </button>
              ))}
            </div>
          </div>

          {/* Time Range */}
          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
              Time Range
            </label>
            <select
              value={dateRange}
              onChange={(e) => setDateRange(e.target.value)}
              className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
            >
              <option value="1h">Last Hour</option>
			  <option value="12h">Last 12 Hours</option>
              <option value="24h">Last 24 Hours</option>
              <option value="7d">Last 7 Days</option>
              <option value="30d">Last 30 Days</option>
              <option value="90d">Last 90 Days</option>
              <option value="custom">Custom Range</option>
            </select>
          </div>

          {/* Aggregation */}
          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
              Aggregation
            </label>
            <select
              value={aggregation}
              onChange={(e) => setAggregation(e.target.value)}
              className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
            >
              <option value="auto">Auto (Smart)</option>
              <option value="1min">1 Minute</option>
              <option value="5min">5 Minutes</option>
              <option value="1hour">1 Hour</option>
              <option value="1day">1 Day</option>
              <option value="1week">1 Week</option>
              <option value="1month">1 Month</option>
              <option value="none">None (Raw Data)</option>
            </select>
          </div>

          {/* Refresh Button */}
          <div className="flex items-end md:col-span-2 lg:col-span-4">
            <button
              onClick={handleRefresh}
              disabled={loading || energyLoading}
              className="w-full px-4 py-2 bg-green-600 hover:bg-green-700 text-white rounded-lg shadow transition-colors disabled:opacity-50"
            >
              {loading || energyLoading ? 'Loading...' : 'Refresh'}
            </button>
          </div>
        </div>

        {/* Custom Date Range */}
        {dateRange === 'custom' && (
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
            <div>
              <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                Start Date
              </label>
              <input
                type="datetime-local"
                value={startDate}
                onChange={(e) => setStartDate(e.target.value)}
                className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
              />
            </div>
            <div>
              <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                End Date
              </label>
              <input
                type="datetime-local"
                value={endDate}
                onChange={(e) => setEndDate(e.target.value)}
                className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
              />
            </div>
          </div>
        )}
      </div>

      {/* Metric Selection */}
      <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
        <div className="flex items-center justify-between mb-4">
          <h3 className="text-lg font-semibold text-gray-900 dark:text-white">Display Options</h3>
          <div className="flex items-center space-x-4">
            <label
              className="flex items-center space-x-2"
              title={energyRestricted ? energyRestrictionMessage : (aggregation === 'none' ? 'Energy data requires aggregation' : '')}
            >
              <input
                type="checkbox"
                checked={showEnergy}
                onChange={(e) => setShowEnergy(e.target.checked)}
                disabled={aggregation === 'none' || energyRestricted}
                className="rounded"
              />
              <span className={`text-sm ${energyRestricted || aggregation === 'none' ? 'text-gray-400 dark:text-gray-500' : 'text-gray-700 dark:text-gray-300'}`}>
                Show Energy & Cost {aggregation === 'none' && '(requires aggregation)'} {energyRestricted && '(aggregation too fine)'}
              </span>
            </label>
            {showEnergy && (
              <div className="flex items-center space-x-2">
                <DollarSign className="h-4 w-4 text-gray-600 dark:text-gray-400" />
                <input
                  type="number"
                  step="0.01"
                  value={costPerKWh}
                  onChange={(e) => setCostPerKWh(parseFloat(e.target.value))}
                  className="w-24 px-2 py-1 rounded border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white text-sm"
                  placeholder="$/kWh"
                />
                <span className="text-sm text-gray-600 dark:text-gray-400">per kWh</span>
              </div>
            )}
          </div>
        </div>
        <div className="flex flex-wrap gap-3">
          {Object.keys(metricConfig).map(metric => (
            <button
              key={metric}
              onClick={() => toggleMetric(metric)}
              className={`px-4 py-2 rounded-lg font-medium transition-colors ${
                selectedMetrics.includes(metric)
                  ? 'bg-blue-600 text-white'
                  : 'bg-gray-200 dark:bg-gray-700 text-gray-700 dark:text-gray-300'
              }`}
            >
              {metricConfig[metric].name}
            </button>
          ))}
        </div>
      </div>

      {/* Auto-Resample Notification */}
      {resampleInfo && (
        <div className="bg-blue-50 dark:bg-blue-900 border-l-4 border-blue-500 p-4 rounded">
          <div className="flex items-start">
            <AlertCircle className="h-5 w-5 text-blue-500 mt-0.5 mr-3 flex-shrink-0" />
            <div className="flex-1">
              <p className="text-sm font-medium text-blue-800 dark:text-blue-200">
                Smart Auto-Aggregation Applied
              </p>
              <p className="text-sm text-blue-700 dark:text-blue-300 mt-1">
                Database contains <strong>{resampleInfo.originalPoints.toLocaleString()}</strong> data points.
                Automatically aggregated to <strong>{resampleInfo.aggregationType}</strong> level
                → <strong>{resampleInfo.finalPoints.toLocaleString()}</strong> points for optimal performance.
              </p>
              <p className="text-xs text-blue-600 dark:text-blue-400 mt-1">
                Select a manual aggregation level in the dropdown to override.
              </p>
            </div>
            <button
              onClick={() => setResampleInfo(null)}
              className="ml-3 text-blue-500 hover:text-blue-700"
            >
              <X className="h-4 w-4" />
            </button>
          </div>
        </div>
      )}

      {/* Charts */}
      <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
        <h3 className="text-lg font-semibold text-gray-900 dark:text-white mb-4 flex items-center">
          <TrendingUp className="h-5 w-5 mr-2" />
          Historical Trends ({Object.values(historicalData).reduce((sum, data) => sum + data.length, 0)} total data points)
        </h3>

        {loading ? (
          <div className="flex items-center justify-center h-96">
            <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-blue-600"></div>
          </div>
        ) : Object.keys(historicalData).length > 0 ? (
          (() => {
            // Helper function: Find nearest point within threshold using binary search
            const findNearestPointWithin = (sortedData, targetTimestamp, threshold) => {
              if (!sortedData || sortedData.length === 0) return null;

              // Binary search for closest timestamp
              let left = 0;
              let right = sortedData.length - 1;
              let closestPoint = null;
              let minDistance = Infinity;

              while (left <= right) {
                const mid = Math.floor((left + right) / 2);
                const point = sortedData[mid];
                const distance = Math.abs(point.timestamp - targetTimestamp);

                if (distance < minDistance) {
                  minDistance = distance;
                  closestPoint = point;
                }

                if (point.timestamp < targetTimestamp) {
                  left = mid + 1;
                } else if (point.timestamp > targetTimestamp) {
                  right = mid - 1;
                } else {
                  // Exact match
                  return point;
                }
              }

              // Check if closest point is within threshold
              return minDistance <= threshold ? closestPoint : null;
            };

            // Time-based merge: collect all unique timestamps from all meters
            const allTimestamps = new Set();
            selectedMeters.forEach(meterId => {
              const meterData = historicalData[meterId] || [];
              meterData.forEach(point => {
                if (point.timestamp) {
                  allTimestamps.add(point.timestamp);
                }
              });
            });

            // Sort timestamps chronologically
            const sortedTimestamps = Array.from(allTimestamps).sort((a, b) => a - b);

            // Keep meter data as sorted arrays for binary search
            const dataByMeter = {};
            selectedMeters.forEach(meterId => {
              const meterData = historicalData[meterId] || [];
              dataByMeter[meterId] = meterData.filter(p => p.timestamp).sort((a, b) => a.timestamp - b.timestamp);
            });

            // Get poll intervals for each meter (for adaptive threshold)
            const pollIntervalByMeter = {};
            selectedMeters.forEach(meterId => {
              const meterConfig = meters.find(m => m.meter_id === meterId);
              pollIntervalByMeter[meterId] = (meterConfig?.poll_interval || 1.0) * 1000; // Convert to ms
            });

            // Create unified data aligned by timestamp with adaptive threshold
            const unifiedData = sortedTimestamps.map(timestamp => {
              const unified = { timestamp };

              // Format time label (use first timestamp for consistency)
              unified.time = format(new Date(timestamp), 'MM/dd hh:mm a');
              unified.fullTime = new Date(timestamp).toLocaleString();

              // Add data from each meter at this timestamp using nearest-neighbor lookup
              selectedMeters.forEach(meterId => {
                const threshold = pollIntervalByMeter[meterId] * 1.5; // Allow 50% jitter/delay
                const nearestPoint = findNearestPointWithin(dataByMeter[meterId], timestamp, threshold);

                if (nearestPoint) {
                  // Meter has data within threshold
                  selectedMetrics.forEach(metric => {
                    unified[`${meterId}_${metric}`] = nearestPoint[metric];
                  });
                } else {
                  // No data within threshold - genuine gap
                  selectedMetrics.forEach(metric => {
                    unified[`${meterId}_${metric}`] = null;
                  });
                }
              });

              return unified;
            });

            return (
              <ResponsiveContainer width="100%" height={550}>
                <LineChart data={unifiedData}>
                  <CartesianGrid strokeDasharray="3 3" stroke="#374151" />
                  <XAxis
                    dataKey="timestamp"
                    type="number"
                    domain={['auto', 'auto']}
                    scale="time"
                    tickFormatter={(ts) => format(new Date(ts), 'MM/dd hh:mm a')}
                    stroke="#9CA3AF"
                    tick={{ fill: '#9CA3AF', fontSize: 12 }}
                    angle={-45}
                    textAnchor="end"
                    height={80}
                  />
                  {selectedMetrics.map((metric, idx) => (
                    <YAxis
                      key={metric}
                      yAxisId={metricConfig[metric].yAxisId}
                      stroke="#9CA3AF"
                      tick={{ fill: '#9CA3AF' }}
                      orientation={idx % 2 === 0 ? 'left' : 'right'}
                    />
                  ))}
                  <Tooltip
                    contentStyle={{
                      backgroundColor: '#1F2937',
                      border: '1px solid #374151',
                      borderRadius: '0.5rem',
                      color: '#F3F4F6'
                    }}
                    labelFormatter={(timestamp) => {
                      if (typeof timestamp === 'number') {
                        return format(new Date(timestamp), 'MM/dd/yyyy hh:mm:ss a');
                      }
                      return timestamp;
                    }}
                    formatter={(value) => {
                      if (typeof value === 'number') {
                        return value.toFixed(3);
                      }
                      return value;
                    }}
                  />
                  <Legend wrapperStyle={{ color: '#9CA3AF' }} />
                  <Brush
                    dataKey="timestamp"
                    tickFormatter={(ts) => format(new Date(ts), 'MM/dd hh:mm a')}
					travellerWidth={15}
                    height={40}
                    stroke="#3B82F6"
                    fill="#374151"
                  />

                  {selectedMeters.map((meterId, meterIdx) => {
                    const meterName = meters.find(m => m.meter_id === meterId)?.name || meterId;

                    return selectedMetrics.map(metric => (
                      <Line
                        key={`${meterId}-${metric}`}
                        type="monotone"
                        dataKey={`${meterId}_${metric}`}
                        stroke={getLineColor(meterIdx, metric)}
                        strokeWidth={2}
                        dot={false}
                        name={`${meterName} - ${metricConfig[metric].name}`}
                        yAxisId={metricConfig[metric].yAxisId}
                        connectNulls={false}
                      />
                    ));
                  })}
                </LineChart>
              </ResponsiveContainer>
            );
          })()
        ) : (
          <div className="flex items-center justify-center h-96 text-gray-500 dark:text-gray-400">
            Click Refresh to load power data
          </div>
        )}
      </div>

      {/* Energy & Cost Chart */}
      {showEnergy && (
        <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
          <h3 className="text-lg font-semibold text-gray-900 dark:text-white mb-4 flex items-center justify-between">
            <span className="flex items-center">
              <DollarSign className="h-5 w-5 mr-2" />
              Energy Consumption & Cost
            </span>
            {Object.keys(energyData).length > 0 && (
              <span className="text-sm font-normal text-gray-600 dark:text-gray-400">
                Total: {(() => {
                  // Use backend-calculated raw totals
                  const totalEnergy = Object.values(energyTotals).reduce((sum, total) => sum + total, 0);
                  const totalCost = totalEnergy * costPerKWh;
                  return `${totalEnergy.toFixed(2)} kWh ($${totalCost.toFixed(2)})`;
                })()}
              </span>
            )}
          </h3>

          {/* Warning for restricted aggregation */}
          {energyRestricted && showEnergyWarning && (
            <div className="bg-yellow-50 dark:bg-yellow-900 border-l-4 border-yellow-500 p-4 rounded mb-4">
              <div className="flex items-start justify-between">
                <div className="flex items-start">
                  <AlertCircle className="h-5 w-5 text-yellow-500 mt-0.5 mr-3 flex-shrink-0" />
                  <div className="flex-1">
                    <p className="text-sm font-medium text-yellow-800 dark:text-yellow-200">
                      Aggregation Level Too Fine
                    </p>
                    <p className="text-sm text-yellow-700 dark:text-yellow-300 mt-1">
                      {energyRestrictionMessage}
                    </p>
                  </div>
                </div>
                <button
                  onClick={() => setShowEnergyWarning(false)}
                  className="text-yellow-500 hover:text-yellow-700 dark:hover:text-yellow-300"
                >
                  <X className="h-5 w-5" />
                </button>
              </div>
            </div>
          )}

          {/* Loading indicator */}
          {energyLoading && (
            <div className="flex items-center justify-center h-64 text-gray-600 dark:text-gray-400">
              <div className="text-center">
                <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-blue-600 mx-auto mb-4"></div>
                <p>Loading energy data...</p>
              </div>
            </div>
          )}

          {/* Empty state */}
          {!energyLoading && !energyRestricted && Object.keys(energyData).length === 0 && (
            <div className="flex items-center justify-center h-64 text-gray-500 dark:text-gray-400">
              <p>Click Refresh to load energy data</p>
            </div>
          )}

          {/* Chart */}
          {!energyLoading && !energyRestricted && Object.keys(energyData).length > 0 && (

          <ResponsiveContainer width="100%" height={400}>
            <BarChart data={(() => {
              // Helper function: Find nearest point within threshold using binary search
              const findNearestPointWithin = (sortedData, targetTimestamp, threshold) => {
                if (!sortedData || sortedData.length === 0) return null;

                // Binary search for closest timestamp
                let left = 0;
                let right = sortedData.length - 1;
                let closestPoint = null;
                let minDistance = Infinity;

                while (left <= right) {
                  const mid = Math.floor((left + right) / 2);
                  const point = sortedData[mid];
                  const distance = Math.abs(point.timestamp - targetTimestamp);

                  if (distance < minDistance) {
                    minDistance = distance;
                    closestPoint = point;
                  }

                  if (point.timestamp < targetTimestamp) {
                    left = mid + 1;
                  } else if (point.timestamp > targetTimestamp) {
                    right = mid - 1;
                  } else {
                    // Exact match
                    return point;
                  }
                }

                // Check if closest point is within threshold
                return minDistance <= threshold ? closestPoint : null;
              };

              // Time-based merge for energy data: collect all unique timestamps
              const allTimestamps = new Set();
              selectedMeters.forEach(meterId => {
                const meterData = energyData[meterId] || [];
                meterData.forEach(point => {
                  if (point.timestamp) {
                    // Energy timestamps are ISO strings, convert to comparable format
                    try {
                      const ts = new Date(point.timestamp);
                      if (!isNaN(ts.getTime())) {
                        allTimestamps.add(ts.getTime());
                      }
                    } catch (e) {
                      console.error('Error parsing energy timestamp:', point.timestamp, e);
                    }
                  }
                });
              });

              // Sort timestamps chronologically
              const sortedTimestamps = Array.from(allTimestamps).sort((a, b) => a - b);

              // Keep meter data as sorted arrays for binary search
              const dataByMeter = {};
              selectedMeters.forEach(meterId => {
                const meterData = energyData[meterId] || [];
                const parsedData = [];
                meterData.forEach(point => {
                  if (point.timestamp) {
                    try {
                      const ts = new Date(point.timestamp);
                      if (!isNaN(ts.getTime())) {
                        parsedData.push({ ...point, timestamp: ts.getTime() });
                      }
                    } catch (e) {
                      console.error('Error parsing energy timestamp:', point.timestamp, e);
                    }
                  }
                });
                dataByMeter[meterId] = parsedData.sort((a, b) => a.timestamp - b.timestamp);
              });

              // Get poll intervals for each meter (for adaptive threshold)
              const pollIntervalByMeter = {};
              selectedMeters.forEach(meterId => {
                const meterConfig = meters.find(m => m.meter_id === meterId);
                pollIntervalByMeter[meterId] = (meterConfig?.poll_interval || 1.0) * 1000; // Convert to ms
              });

              // Create unified data aligned by timestamp with adaptive threshold
              return sortedTimestamps.map(timestamp => {
                // Format timestamp for display
                const localTime = format(new Date(timestamp), 'MM/dd hh:mm a');
                const unified = { time: localTime, timestamp: timestamp };

                // Add data from each meter at this timestamp using nearest-neighbor lookup
                selectedMeters.forEach(meterId => {
                  const threshold = pollIntervalByMeter[meterId] * 1.5; // Allow 50% jitter/delay
                  const nearestPoint = findNearestPointWithin(dataByMeter[meterId], timestamp, threshold);

                  if (nearestPoint) {
                    // Meter has data within threshold
                    const energyKwh = nearestPoint.energy_kwh;
                    unified[`${meterId}_energy`] = energyKwh;
                    unified[`${meterId}_cost`] = energyKwh !== undefined && energyKwh !== null ? energyKwh * costPerKWh : undefined;
                  } else {
                    // No data within threshold - genuine gap
                    unified[`${meterId}_energy`] = null;
                    unified[`${meterId}_cost`] = null;
                  }
                });

                return unified;
              });
            })()}>
              <CartesianGrid strokeDasharray="3 3" stroke="#374151" />
              <XAxis
                dataKey="timestamp"
                type="number"
                domain={['auto', 'auto']}
                scale="time"
                tickFormatter={(ts) => format(new Date(ts), 'MM/dd hh:mm a')}
                stroke="#9CA3AF"
                tick={{ fill: '#9CA3AF', fontSize: 12 }}
                angle={-45}
                textAnchor="end"
                height={80}
              />
              <YAxis
                stroke="#9CA3AF"
                tick={{ fill: '#9CA3AF' }}
                label={{ value: 'Energy (kWh)', angle: -90, position: 'insideLeft', fill: '#9CA3AF', offset: 10 }}
              />
              <Tooltip
                contentStyle={{
                  backgroundColor: '#1F2937',
                  border: '1px solid #374151',
                  borderRadius: '0.5rem',
                  color: '#F3F4F6'
                }}
                labelFormatter={(timestamp) => {
                  if (typeof timestamp === 'number') {
                    return format(new Date(timestamp), 'MM/dd/yyyy hh:mm:ss a');
                  }
                  return timestamp;
                }}
                formatter={(value, name) => {
                  // value = energy in kWh from the bar being hovered
                  // name = meter name from the Bar's name prop

                  // Handle undefined/null values
                  if (value === undefined || value === null || isNaN(value)) {
                    return ['N/A'];
                  }

                  // Calculate cost using current costPerKWh value
                  const cost = value * costPerKWh;

                  // Calculate Watt-minutes for sub-hour aggregations
                  // 1 kWh = 1000 Wh = 60,000 Watt-minutes
                  const wattMinutes = value * 60000;

                  // Check if we should show Watt-minutes and cents (for any aggregation under 1 hour)
                  const isSubHourAggregation = appliedAggregation && appliedAggregation.endsWith('min');

                  return [
                    <div key={`tooltip-${name}`}>
                      <div style={{ fontWeight: 'bold', marginBottom: '4px' }}>{name}</div>
                      <div>Energy: {value.toFixed(3)} kWh</div>
                      {isSubHourAggregation && (
                        <div>Watt-minutes: {wattMinutes.toFixed(2)} Wmin</div>
                      )}
                      {isSubHourAggregation ? (
                        <div>Cost: {(cost * 100).toFixed(3)}¢</div>
                      ) : (
                        <div>Cost: ${cost.toFixed(3)}</div>
                      )}
                    </div>
                  ];
                }}
                labelStyle={{ color: '#F3F4F6' }}
              />
              <Legend wrapperStyle={{ color: '#9CA3AF' }} />

              {selectedMeters.map((meterId, idx) => {
                const meterName = meters.find(m => m.meter_id === meterId)?.name || meterId;

                return (
                  <Bar
                    key={`${meterId}-energy`}
                    dataKey={`${meterId}_energy`}
                    fill={getBarColor(idx, 0)}
                    name={`${meterName}`}
                  />
                );
              })}
            </BarChart>
          </ResponsiveContainer>
          )}
        </div>
      )}

      {/* Energy Statistics */}
      {showEnergy && !energyLoading && !energyRestricted && Object.keys(energyData).length > 0 && (
        <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
          <h3 className="text-lg font-semibold text-gray-900 dark:text-white mb-4">Energy Statistics</h3>

          {selectedMeters.map(meterId => {
            const meterName = meters.find(m => m.meter_id === meterId)?.name || meterId;
            const data = energyData[meterId] || [];

            if (data.length === 0) return null;

            // Use backend-calculated raw total
            const totalEnergy = energyTotals[meterId] || 0;
            const totalCost = totalEnergy * costPerKWh;  // Recalculate using current costPerKWh
            const avgEnergy = totalEnergy / data.length;
            const maxEnergy = Math.max(...data.map(d => d.energy_kwh || 0));
            const minEnergy = Math.min(...data.map(d => d.energy_kwh || 0));

            return (
              <div key={meterId} className="mb-6 last:mb-0">
                <h4 className="text-md font-medium text-gray-800 dark:text-gray-200 mb-3">{meterName}</h4>
                <div className="grid grid-cols-2 md:grid-cols-5 gap-4">
                  <div className="p-4 bg-gray-50 dark:bg-gray-700 rounded-lg">
                    <p className="text-sm text-gray-600 dark:text-gray-400 mb-2">Total Energy</p>
                    <p className="text-2xl font-bold text-gray-900 dark:text-white">
                      {totalEnergy.toFixed(3)} <span className="text-sm font-normal">kWh</span>
                    </p>
                  </div>
                  <div className="p-4 bg-gray-50 dark:bg-gray-700 rounded-lg">
                    <p className="text-sm text-gray-600 dark:text-gray-400 mb-2">Total Cost</p>
                    <p className="text-2xl font-bold text-gray-900 dark:text-white">
                      ${totalCost.toFixed(2)}
                    </p>
                  </div>
                  <div className="p-4 bg-gray-50 dark:bg-gray-700 rounded-lg">
                    <p className="text-sm text-gray-600 dark:text-gray-400 mb-2">Avg per Period</p>
                    <p className="text-lg font-bold text-gray-900 dark:text-white">
                      {avgEnergy.toFixed(3)} <span className="text-sm font-normal">kWh</span>
                    </p>
                  </div>
                  <div className="p-4 bg-gray-50 dark:bg-gray-700 rounded-lg">
                    <p className="text-sm text-gray-600 dark:text-gray-400 mb-2">Max per Period</p>
                    <p className="text-lg font-bold text-gray-900 dark:text-white">
                      {maxEnergy.toFixed(3)} <span className="text-sm font-normal">kWh</span>
                    </p>
                  </div>
                  <div className="p-4 bg-gray-50 dark:bg-gray-700 rounded-lg">
                    <p className="text-sm text-gray-600 dark:text-gray-400 mb-2">Min per Period</p>
                    <p className="text-lg font-bold text-gray-900 dark:text-white">
                      {minEnergy.toFixed(3)} <span className="text-sm font-normal">kWh</span>
                    </p>
                  </div>
                </div>
              </div>
            );
          })}
        </div>
      )}

      {/* Statistics Summary */}
      {Object.keys(historicalData).length > 0 && (
        <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
          <h3 className="text-lg font-semibold text-gray-900 dark:text-white mb-4">Statistics Summary</h3>

          {selectedMeters.map(meterId => {
            const meterName = meters.find(m => m.meter_id === meterId)?.name || meterId;
            const data = historicalData[meterId] || [];

            if (data.length === 0) return null;

            return (
              <div key={meterId} className="mb-6 last:mb-0">
                <h4 className="text-md font-medium text-gray-800 dark:text-gray-200 mb-3">{meterName}</h4>
                <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
                  {selectedMetrics.map(metric => {
                    const values = data.map(d => d[metric]).filter(v => v !== undefined && v !== null);
                    if (values.length === 0) return null;

                    const avg = values.reduce((a, b) => a + b, 0) / values.length;
                    const max = Math.max(...values);
                    const min = Math.min(...values);

                    return (
                      <div key={metric} className="p-4 bg-gray-50 dark:bg-gray-700 rounded-lg">
                        <p className="text-sm text-gray-600 dark:text-gray-400 mb-2">
                          {metricConfig[metric].name}
                        </p>
                        <div className="space-y-1 text-xs">
                          <p><span className="font-medium">Avg:</span> {avg.toFixed(2)}</p>
                          <p><span className="font-medium">Max:</span> {max.toFixed(2)}</p>
                          <p><span className="font-medium">Min:</span> {min.toFixed(2)}</p>
                        </div>
                      </div>
                    );
                  })}
                </div>
              </div>
            );
          })}
        </div>
      )}

      {/* Large Dataset Warning Modal */}
      {showLargeDatasetWarning && largeDatasetInfo && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
          <div className="bg-white dark:bg-gray-800 rounded-lg shadow-xl p-6 max-w-md mx-4">
            <div className="flex items-start mb-4">
              <AlertCircle className="h-6 w-6 text-yellow-500 mt-0.5 mr-3 flex-shrink-0" />
              <div className="flex-1">
                <h3 className="text-lg font-semibold text-gray-900 dark:text-white">
                  Large Dataset Warning
                </h3>
              </div>
            </div>

            <div className="mb-6 text-gray-700 dark:text-gray-300">
              <p className="mb-2">
                This query will return <strong>{largeDatasetInfo.count.toLocaleString()}</strong> data points
                (threshold: 10,000 points).
              </p>
              <p className="mb-2">
                This may slow down your browser and take significant time to load.
              </p>
              <ul className="list-disc list-inside space-y-1 text-sm mt-3">
                <li>Estimated load time: ~{largeDatasetInfo.timeEstimate} seconds</li>
                <li>Data size: ~{largeDatasetInfo.sizeEstimateMB} MB</li>
              </ul>
            </div>

            <div className="flex space-x-3">
              <button
                onClick={() => {
                  setShowLargeDatasetWarning(false);
                  setLargeDatasetInfo(null);
                }}
                className="flex-1 px-4 py-2 bg-gray-200 dark:bg-gray-700 text-gray-800 dark:text-gray-200 rounded-lg hover:bg-gray-300 dark:hover:bg-gray-600 transition-colors"
              >
                Cancel
              </button>
              <button
                onClick={async () => {
                  setShowLargeDatasetWarning(false);
                  await fetchHistoricalData(true); // Skip warning check
                  if (showEnergy && !energyRestricted) {
                    await fetchEnergyData();
                  }
                }}
                className="flex-1 px-4 py-2 bg-yellow-600 hover:bg-yellow-700 text-white rounded-lg transition-colors"
              >
                Load Anyway
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default HistoricalView;