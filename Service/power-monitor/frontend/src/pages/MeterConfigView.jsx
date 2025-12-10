import React, { useState, useEffect } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { ArrowLeft, Save, Download, Upload, Zap, AlertCircle, RefreshCw, Plus, X } from 'lucide-react';

// Common timezone presets that convert to the meter's DST/STD format
const TIMEZONE_PRESETS = {
  'US/Eastern': {
    name: 'US Eastern (EST/EDT)',
    dst: { abbrev: 'EDT', week: 2, dow: 1, month: 3, hour: 2, offset: -240 },
    std: { abbrev: 'EST', week: 1, dow: 1, month: 11, hour: 2, offset: -300 }
  },
  'US/Central': {
    name: 'US Central (CST/CDT)',
    dst: { abbrev: 'CDT', week: 2, dow: 1, month: 3, hour: 2, offset: -300 },
    std: { abbrev: 'CST', week: 1, dow: 1, month: 11, hour: 2, offset: -360 }
  },
  'US/Mountain': {
    name: 'US Mountain (MST/MDT)',
    dst: { abbrev: 'MDT', week: 2, dow: 1, month: 3, hour: 2, offset: -360 },
    std: { abbrev: 'MST', week: 1, dow: 1, month: 11, hour: 2, offset: -420 }
  },
  'US/Pacific': {
    name: 'US Pacific (PST/PDT)',
    dst: { abbrev: 'PDT', week: 2, dow: 1, month: 3, hour: 2, offset: -420 },
    std: { abbrev: 'PST', week: 1, dow: 1, month: 11, hour: 2, offset: -480 }
  },
  'US/Alaska': {
    name: 'US Alaska (AKST/AKDT)',
    dst: { abbrev: 'AKDT', week: 2, dow: 1, month: 3, hour: 2, offset: -480 },
    std: { abbrev: 'AKST', week: 1, dow: 1, month: 11, hour: 2, offset: -540 }
  },
  'US/Hawaii': {
    name: 'US Hawaii (HST)',
    dst: { abbrev: 'HST', week: 0, dow: 0, month: 0, hour: 0, offset: -600 },
    std: { abbrev: 'HST', week: 0, dow: 0, month: 0, hour: 0, offset: -600 }
  },
  'Europe/London': {
    name: 'UK (GMT/BST)',
    dst: { abbrev: 'BST', week: 5, dow: 1, month: 3, hour: 1, offset: 60 },
    std: { abbrev: 'GMT', week: 5, dow: 1, month: 10, hour: 2, offset: 0 }
  },
  'Europe/Paris': {
    name: 'Central Europe (CET/CEST)',
    dst: { abbrev: 'CEST', week: 5, dow: 1, month: 3, hour: 2, offset: 120 },
    std: { abbrev: 'CET', week: 5, dow: 1, month: 10, hour: 3, offset: 60 }
  },
  'Asia/Tokyo': {
    name: 'Japan (JST)',
    dst: { abbrev: 'JST', week: 0, dow: 0, month: 0, hour: 0, offset: 540 },
    std: { abbrev: 'JST', week: 0, dow: 0, month: 0, hour: 0, offset: 540 }
  },
  'Australia/Sydney': {
    name: 'Australia Sydney (AEDT/AEST)',
    dst: { abbrev: 'AEDT', week: 1, dow: 1, month: 10, hour: 2, offset: 660 },
    std: { abbrev: 'AEST', week: 1, dow: 1, month: 4, hour: 3, offset: 600 }
  },
  'custom': {
    name: 'Custom Configuration',
    dst: { abbrev: 'DST', week: 2, dow: 1, month: 3, hour: 2, offset: 0 },
    std: { abbrev: 'STD', week: 1, dow: 1, month: 11, hour: 2, offset: 0 }
  }
};


const MeterConfigView = ({ meters }) => {
  const { meterId } = useParams();
  const navigate = useNavigate();
  
  const [loadError, setLoadError] = useState(null);
  const [meter, setMeter] = useState(null);
  const [meterSettings, setMeterSettings] = useState(null);
  const [calibrationSettings, setCalibrationSettings] = useState(null);
  const [availableRegisters, setAvailableRegisters] = useState([]);
  const [loading, setLoading] = useState(false);
  const [saveMessage, setSaveMessage] = useState('');
  const [activeSection, setActiveSection] = useState('rtc');
  const [selectedTimezone, setSelectedTimezone] = useState('US/Central');
  
  const [autoCalForm, setAutoCalForm] = useState({
    phase: 'A',
    type: 'voltage',
    expected: '',
    measured: ''
  });

  const [energyCalForm, setEnergyCalForm] = useState({
    phaseA: false,
    phaseB: false,
    phaseC: false,
    loadWatts: '',
    durationMinutes: '',
    calibratingPhase: null,
    calibrationStarted: false
  });

  useEffect(() => {
    const foundMeter = meters.find(m => m.meter_id === meterId);
    if (foundMeter) {
      setMeter(foundMeter);
      fetchAllSettings(foundMeter);
      fetchAvailableRegisters(foundMeter);
    }
  }, [meterId, meters]);

  const fetchAvailableRegisters = async (meterData) => {
    try {
      const response = await fetch(`/api/meters/${meterData.meter_id}/proxy/api/registers`);
      const data = await response.json();
      if (data.success) {
        // Filter to only readable registers that make sense for display/logging
        const filtered = data.registers.filter(r => 
          r.access !== 'write' && r.unit && r.unit !== ''
        );
        setAvailableRegisters(filtered);
      }
    } catch (error) {
      console.error('Error fetching registers:', error);
    }
  };

  const fetchAllSettings = async (meterData) => {
    setLoading(true);
    try {
      await Promise.all([
        fetchMeterSettings(meterData),
        fetchCalibrationSettings(meterData)
      ]);
    } finally {
      setLoading(false);
    }
  };

  const fetchMeterSettings = async (meterData) => {
    try {
      const response = await fetch(`/api/meters/${meterData.meter_id}/proxy/api/settings`);
      const data = await response.json();
      if (data.success) {
        setMeterSettings(data);
        // Try to detect which timezone preset matches
        detectTimezonePreset(data.timezone);
      }
    } catch (error) {
      console.error('Error fetching meter settings:', error);
	  setLoadError(error.message || 'Unknown error');
      showMessage('Error fetching meter settings', true);
    }
  };

  const detectTimezonePreset = (tz) => {
    if (!tz) return;
    for (const [key, preset] of Object.entries(TIMEZONE_PRESETS)) {
      if (preset.dst.abbrev === tz.dstAbbrev && preset.std.abbrev === tz.stdAbbrev &&
          preset.dst.offset === tz.dstOffset && preset.std.offset === tz.stdOffset) {
        setSelectedTimezone(key);
        return;
      }
    }
    setSelectedTimezone('custom');
  };

  const fetchCalibrationSettings = async (meterData) => {
    try {
      const response = await fetch(`/api/meters/${meterData.meter_id}/proxy/api/settings/calibration`);
      const data = await response.json();
      if (data.success) {
        setCalibrationSettings(data);
      }
    } catch (error) {
      console.error('Error fetching calibration:', error);
      showMessage('Error fetching calibration settings', true);
    }
  };

  const applyTimezonePreset = (presetKey) => {
    setSelectedTimezone(presetKey);
    const preset = TIMEZONE_PRESETS[presetKey];
    if (preset && meterSettings) {
      setMeterSettings({
        ...meterSettings,
        timezone: {
          dstAbbrev: preset.dst.abbrev,
          dstWeek: preset.dst.week,
          dstDow: preset.dst.dow,
          dstMonth: preset.dst.month,
          dstHour: preset.dst.hour,
          dstOffset: preset.dst.offset,
          stdAbbrev: preset.std.abbrev,
          stdWeek: preset.std.week,
          stdDow: preset.std.dow,
          stdMonth: preset.std.month,
          stdHour: preset.std.hour,
          stdOffset: preset.std.offset
        }
      });
    }
  };

  const addLogField = () => {
    if (!meterSettings?.dataLogging) return;
    const currentFields = meterSettings.dataLogging.logFields.split(',').filter(f => f.trim());
    if (currentFields.length === 0 || currentFields[currentFields.length - 1] !== '') {
      currentFields.push('UrmsA');
      setMeterSettings({
        ...meterSettings,
        dataLogging: {
          ...meterSettings.dataLogging,
          logFields: currentFields.join(',')
        }
      });
    }
  };

  const removeLogField = (index) => {
    if (!meterSettings?.dataLogging) return;
    const fields = meterSettings.dataLogging.logFields.split(',').filter(f => f.trim());
    fields.splice(index, 1);
    setMeterSettings({
      ...meterSettings,
      dataLogging: {
        ...meterSettings.dataLogging,
        logFields: fields.join(',')
      }
    });
  };

  const updateLogField = (index, value) => {
    if (!meterSettings?.dataLogging) return;
    const fields = meterSettings.dataLogging.logFields.split(',').filter(f => f.trim());
    fields[index] = value;
    setMeterSettings({
      ...meterSettings,
      dataLogging: {
        ...meterSettings.dataLogging,
        logFields: fields.join(',')
      }
    });
  };

  const saveMeterSettings = async () => {
    if (!meter || !meterSettings) return;
    
    setLoading(true);
    try {
      const response = await fetch(`/api/meters/${meter.meter_id}/proxy/api/settings`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(meterSettings)
      });
      const data = await response.json();
      if (data.success) {
        showMessage('Settings updated on meter');
      } else {
        showMessage('Failed to update settings', true);
      }
    } catch (error) {
      showMessage('Error saving settings', true);
    } finally {
      setLoading(false);
    }
  };

  const saveCalibration = async (applyToChip = false) => {
    if (!meter || !calibrationSettings) return;
    
    setLoading(true);
    try {
      const payload = { ...calibrationSettings, applyToChip };
      delete payload.success;
      
      const response = await fetch(`/api/meters/${meter.meter_id}/proxy/api/settings/calibration`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      const data = await response.json();
      if (data.success) {
        showMessage(data.message || 'Calibration updated');
        fetchCalibrationSettings(meter);
      } else {
        showMessage('Failed to update calibration', true);
      }
    } catch (error) {
      showMessage('Error saving calibration', true);
    } finally {
      setLoading(false);
    }
  };

  const autoCalibrate = async () => {
    if (!meter || !autoCalForm.expected || !autoCalForm.measured) {
      showMessage('Please fill in all fields', true);
      return;
    }

    setLoading(true);
    try {
      const response = await fetch(`/api/meters/${meter.meter_id}/proxy/api/calibrate`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          phase: autoCalForm.phase,
          type: autoCalForm.type,
          expected: parseFloat(autoCalForm.expected),
          measured: parseFloat(autoCalForm.measured)
        })
      });
      const data = await response.json();
      if (data.success) {
        showMessage(`Calibration complete! ${data.message}`);
        fetchCalibrationSettings(meter);
        setAutoCalForm({ ...autoCalForm, expected: '', measured: '' });
      } else {
        showMessage('Auto-calibration failed', true);
      }
    } catch (error) {
      showMessage('Error during auto-calibration', true);
    } finally {
      setLoading(false);
    }
  };

  const startEnergyCalibration = async () => {
    if (!meter) return;

    // Build phases string from checkboxes
    let phases = '';
    if (energyCalForm.phaseA) phases += 'A';
    if (energyCalForm.phaseB) phases += 'B';
    if (energyCalForm.phaseC) phases += 'C';

    if (!phases) {
      showMessage('Please select at least one phase', true);
      return;
    }

    setLoading(true);
    try {
      const response = await fetch(`/api/meters/${meter.meter_id}/energy/calibrate/start`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ phases })
      });
      const data = await response.json();
      if (data.success) {
        showMessage('Energy calibration started! Connect known load and enter values below.');
        setEnergyCalForm({
          ...energyCalForm,
          calibrationStarted: true,
          calibratingPhase: phases.length === 1 ? phases : null
        });
      } else {
        showMessage('Failed to start energy calibration', true);
      }
    } catch (error) {
      showMessage('Error starting energy calibration', true);
    } finally {
      setLoading(false);
    }
  };

  const completeEnergyCalibration = async (phase) => {
    if (!meter || !energyCalForm.loadWatts || !energyCalForm.durationMinutes) {
      showMessage('Please fill in load watts and duration', true);
      return;
    }

    setLoading(true);
    try {
      const response = await fetch(`/api/meters/${meter.meter_id}/energy/calibrate/complete`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          phase,
          loadWatts: parseFloat(energyCalForm.loadWatts),
          durationMinutes: parseFloat(energyCalForm.durationMinutes)
        })
      });
      const data = await response.json();
      if (data.success) {
        showMessage(`Energy calibration complete for phase ${phase}! ${data.message || ''}`);
        // Reset form
        setEnergyCalForm({
          phaseA: false,
          phaseB: false,
          phaseC: false,
          loadWatts: '',
          durationMinutes: '',
          calibratingPhase: null,
          calibrationStarted: false
        });
      } else {
        showMessage(`Energy calibration failed for phase ${phase}`, true);
      }
    } catch (error) {
      showMessage('Error completing energy calibration', true);
    } finally {
      setLoading(false);
    }
  };

  const saveToPermanent = async () => {
    if (!meter) return;
    
    setLoading(true);
    try {
      const response = await fetch(`/api/meters/${meter.meter_id}/proxy/api/settings/save`, {
        method: 'POST'
      });
      const data = await response.json();
      if (data.success) {
        showMessage('Settings saved to SD card');
      } else {
        showMessage('Failed to save to SD card', true);
      }
    } catch (error) {
      showMessage('Error saving to SD card', true);
    } finally {
      setLoading(false);
    }
  };

  const reloadFromPermanent = async () => {
    if (!meter) return;
    
    setLoading(true);
    try {
      const response = await fetch(`/api/meters/${meter.meter_id}/proxy/api/settings/reload`, {
        method: 'POST'
      });
      
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      
      const data = await response.json();
      if (data.success) {
        showMessage('Settings reloaded from SD card, fetching updated settings...');
        await new Promise(resolve => setTimeout(resolve, 500));
        await fetchAllSettings(meter);
        showMessage('Settings reloaded and refreshed successfully!');
      } else {
        showMessage('Failed to reload from SD card', true);
      }
    } catch (error) {
      console.error('Reload error:', error);
      showMessage(`Error reloading from SD card: ${error.message}`, true);
    } finally {
      setLoading(false);
    }
  };

  const showMessage = (message, isError = false) => {
    setSaveMessage({ text: message, isError });
    setTimeout(() => setSaveMessage(''), 3000);
  };

  const logFields = meterSettings?.dataLogging?.logFields 
    ? meterSettings.dataLogging.logFields.split(',').filter(f => f.trim())
    : [];
	
if (!meter) {
  return (
    <div className="min-h-screen bg-gray-50 dark:bg-gray-900 p-6 flex items-center justify-center text-gray-800 dark:text-gray-200">
      Meter not found or invalid meterId.
    </div>
  );
}

if (loading) {
  return (
    <div className="min-h-screen bg-gray-50 dark:bg-gray-900 p-6 flex items-center justify-center">
      <RefreshCw className="h-10 w-10 animate-spin text-blue-600" />
    </div>
  );
}

if (!meterSettings) {
  return (
    <div className="min-h-screen bg-gray-50 dark:bg-gray-900 p-6 flex items-center justify-center text-red-700 dark:text-red-300">
      Unable to communicate with meter — could not load settings.
    </div>
  );
}

if (!meterSettings && loadError && !loading) {
  return (
    <div className="min-h-screen bg-gray-50 dark:bg-gray-900 p-6 flex flex-col items-center justify-center text-red-700 dark:text-red-300">
      <div className="text-lg font-semibold mb-2">
        Unable to communicate with meter — could not load settings.
      </div>
      <div className="text-sm opacity-80">
        {loadError}
      </div>
    </div>
  );
}

const backToSettings = () => {
    navigate(`/settings`);
  };



  return (
    <div className="min-h-screen bg-gray-50 dark:bg-gray-900 p-6">
      <div className="max-w-6xl mx-auto space-y-6">
        {/* Header */}
        <div className="flex items-center justify-between">
          <div className="flex items-center space-x-4">
            <button
              className="p-2 text-gray-600 dark:text-gray-400 hover:bg-gray-100 dark:hover:bg-gray-700 rounded transition-colors"
			  onClick={backToSettings}
			>
              <ArrowLeft className="h-6 w-6" />
            </button>
            <div>
              <h1 className="text-3xl font-bold text-gray-900 dark:text-white">
                {meter.name}
              </h1>
              <p className="text-sm text-gray-600 dark:text-gray-400">
                {meter.ip_address} - {meter.location}
              </p>
            </div>
          </div>
          
          <div className="flex space-x-2">
            <button
              onClick={reloadFromPermanent}
              disabled={loading}
              className="inline-flex items-center px-4 py-2 bg-gray-600 hover:bg-gray-700 text-white rounded-lg shadow transition-colors disabled:opacity-50"
            >
              <Upload className="h-5 w-5 mr-2" />
              Reload from SD
            </button>
            <button
              onClick={saveToPermanent}
              disabled={loading}
              className="inline-flex items-center px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg shadow transition-colors disabled:opacity-50"
            >
              <Download className="h-5 w-5 mr-2" />
              Save to SD
            </button>
          </div>
        </div>

        {/* Save Message */}
        {saveMessage && (
          <div className={`p-4 rounded-lg flex items-center ${
            saveMessage.isError 
              ? 'bg-red-100 dark:bg-red-900 text-red-700 dark:text-red-200'
              : 'bg-green-100 dark:bg-green-900 text-green-700 dark:text-green-200'
          }`}>
            <AlertCircle className="h-5 w-5 mr-2" />
            {saveMessage.text}
          </div>
        )}

        {/* Tabs */}
        <div className="bg-white dark:bg-gray-800 rounded-lg shadow overflow-x-auto">
          <div className="flex border-b border-gray-200 dark:border-gray-700 min-w-max">
            {[
              { id: 'rtc', label: 'RTC Calibration' },
              { id: 'timezone', label: 'Timezone' },
              { id: 'logging', label: 'Data Logging' },
              { id: 'display', label: 'Display' },
              { id: 'system', label: 'System' },
              { id: 'calibration', label: 'Calibration' },
              { id: 'auto-cal', label: 'Auto Calibrate', icon: Zap }
            ].map(tab => (
              <button
                key={tab.id}
                onClick={() => setActiveSection(tab.id)}
                className={`px-6 py-3 font-medium transition-colors whitespace-nowrap ${
                  activeSection === tab.id
                    ? 'border-b-2 border-blue-600 text-blue-600 dark:text-blue-400'
                    : 'text-gray-600 dark:text-gray-400 hover:text-gray-900 dark:hover:text-gray-200'
                }`}
              >
                {tab.icon && <tab.icon className="inline h-4 w-4 mr-1" />}
                {tab.label}
              </button>
            ))}
          </div>
        </div>

        {loading && (
          <div className="flex items-center justify-center py-12">
            <RefreshCw className="h-8 w-8 animate-spin text-blue-600" />
          </div>
        )}

        {/* RTC Calibration Section */}
        {!loading && activeSection === 'rtc' && meterSettings?.rtcCalibration && (
          <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
            <h2 className="text-xl font-semibold text-gray-900 dark:text-white mb-4">RTC Calibration Settings</h2>
            <div className="space-y-6">
              <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">NTP Server</label>
                  <input
                    type="text"
                    value={meterSettings.rtcCalibration.ntpServer || ''}
                    onChange={(e) => setMeterSettings({
                      ...meterSettings,
                      rtcCalibration: { ...meterSettings.rtcCalibration, ntpServer: e.target.value }
                    })}
                    className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Min Calibration Days</label>
                  <input
                    type="number"
                    value={meterSettings.rtcCalibration.minCalibrationDays || ''}
                    onChange={(e) => setMeterSettings({
                      ...meterSettings,
                      rtcCalibration: { ...meterSettings.rtcCalibration, minCalibrationDays: parseInt(e.target.value) }
                    })}
                    className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Calibration Threshold (seconds)</label>
                  <input
                    type="number"
                    step="0.1"
                    value={meterSettings.rtcCalibration.calibrationThreshold || ''}
                    onChange={(e) => setMeterSettings({
                      ...meterSettings,
                      rtcCalibration: { ...meterSettings.rtcCalibration, calibrationThreshold: parseFloat(e.target.value) }
                    })}
                    className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                  />
                </div>
              </div>

              <div className="flex flex-col space-y-3">
                <label className="flex items-center space-x-2">
                  <input
                    type="checkbox"
                    checked={meterSettings.rtcCalibration.autoCalibrationEnabled || false}
                    onChange={(e) => setMeterSettings({
                      ...meterSettings,
                      rtcCalibration: { ...meterSettings.rtcCalibration, autoCalibrationEnabled: e.target.checked }
                    })}
                    className="w-4 h-4 text-blue-600 rounded"
                  />
                  <span className="text-sm text-gray-700 dark:text-gray-300">Auto Calibration Enabled</span>
                </label>
                <label className="flex items-center space-x-2">
                  <input
                    type="checkbox"
                    checked={meterSettings.rtcCalibration.calibrationEnabled || false}
                    onChange={(e) => setMeterSettings({
                      ...meterSettings,
                      rtcCalibration: { ...meterSettings.rtcCalibration, calibrationEnabled: e.target.checked }
                    })}
                    className="w-4 h-4 text-blue-600 rounded"
                  />
                  <span className="text-sm text-gray-700 dark:text-gray-300">Calibration Enabled</span>
                </label>
              </div>

              <div className="grid grid-cols-1 md:grid-cols-2 gap-4 pt-4 border-t border-gray-200 dark:border-gray-700">
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Last Calibration Time</label>
                  <div className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-gray-50 dark:bg-gray-900 text-gray-600 dark:text-gray-400">
                    {meterSettings.rtcCalibration.lastCalibrationTime || 'Never'}
                  </div>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Current Offset (ppm)</label>
                  <div className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-gray-50 dark:bg-gray-900 text-gray-600 dark:text-gray-400">
                    {meterSettings.rtcCalibration.currentOffset || '0'}
                  </div>
                </div>
              </div>

              <button
                onClick={saveMeterSettings}
                disabled={loading}
                className="inline-flex items-center px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg shadow transition-colors disabled:opacity-50"
              >
                <Save className="h-5 w-5 mr-2" />
                Save RTC Settings
              </button>
            </div>
          </div>
        )}

        {/* Timezone Section */}
        {!loading && activeSection === 'timezone' && meterSettings?.timezone && (
          <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
            <h2 className="text-xl font-semibold text-gray-900 dark:text-white mb-4">Timezone Settings</h2>
            <div className="space-y-6">
              <div>
                <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Select Timezone Preset</label>
                <select
                  value={selectedTimezone}
                  onChange={(e) => applyTimezonePreset(e.target.value)}
                  className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                >
                  {Object.entries(TIMEZONE_PRESETS).map(([key, preset]) => (
                    <option key={key} value={key}>{preset.name}</option>
                  ))}
                </select>
                <p className="text-xs text-gray-500 dark:text-gray-400 mt-1">
                  Select 'Custom Configuration' to manually edit DST/STD settings below
                </p>
              </div>

              {selectedTimezone === 'custom' && (
                <>
                  <div>
                    <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-3">Daylight Saving Time</h3>
                    <div className="grid grid-cols-2 md:grid-cols-3 gap-4">
                      <div>
                        <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Abbreviation</label>
                        <input
                          type="text"
                          value={meterSettings.timezone.dstAbbrev || ''}
                          onChange={(e) => setMeterSettings({
                            ...meterSettings,
                            timezone: { ...meterSettings.timezone, dstAbbrev: e.target.value }
                          })}
                          className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        />
                      </div>
                      <div>
                        <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Week (1-5, 5=Last)</label>
                        <input
                          type="number"
                          min="1"
                          max="5"
                          value={meterSettings.timezone.dstWeek || ''}
                          onChange={(e) => setMeterSettings({
                            ...meterSettings,
                            timezone: { ...meterSettings.timezone, dstWeek: parseInt(e.target.value) }
                          })}
                          className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        />
                      </div>
                      <div>
                        <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Day (1=Sun-7=Sat)</label>
                        <input
                          type="number"
                          min="1"
                          max="7"
                          value={meterSettings.timezone.dstDow || ''}
                          onChange={(e) => setMeterSettings({
                            ...meterSettings,
                            timezone: { ...meterSettings.timezone, dstDow: parseInt(e.target.value) }
                          })}
                          className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        />
                      </div>
                      <div>
                        <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Month (1-12)</label>
                        <input
                          type="number"
                          min="1"
                          max="12"
                          value={meterSettings.timezone.dstMonth || ''}
                          onChange={(e) => setMeterSettings({
                            ...meterSettings,
                            timezone: { ...meterSettings.timezone, dstMonth: parseInt(e.target.value) }
                          })}
                          className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        />
                      </div>
                      <div>
                        <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Hour (0-23)</label>
                        <input
                          type="number"
                          min="0"
                          max="23"
                          value={meterSettings.timezone.dstHour || ''}
                          onChange={(e) => setMeterSettings({
                            ...meterSettings,
                            timezone: { ...meterSettings.timezone, dstHour: parseInt(e.target.value) }
                          })}
                          className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        />
                      </div>
                      <div>
                        <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">UTC Offset (min)</label>
                        <input
                          type="number"
                          value={meterSettings.timezone.dstOffset || ''}
                          onChange={(e) => setMeterSettings({
                            ...meterSettings,
                            timezone: { ...meterSettings.timezone, dstOffset: parseInt(e.target.value) }
                          })}
                          className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        />
                      </div>
                    </div>
                  </div>

                  <div>
                    <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-3">Standard Time</h3>
                    <div className="grid grid-cols-2 md:grid-cols-3 gap-4">
                      <div>
                        <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Abbreviation</label>
                        <input
                          type="text"
                          value={meterSettings.timezone.stdAbbrev || ''}
                          onChange={(e) => setMeterSettings({
                            ...meterSettings,
                            timezone: { ...meterSettings.timezone, stdAbbrev: e.target.value }
                          })}
                          className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        />
                      </div>
                      <div>
                        <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Week (1-5, 5=Last)</label>
                        <input
                          type="number"
                          min="1"
                          max="5"
                          value={meterSettings.timezone.stdWeek || ''}
                          onChange={(e) => setMeterSettings({
                            ...meterSettings,
                            timezone: { ...meterSettings.timezone, stdWeek: parseInt(e.target.value) }
                          })}
                          className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        />
                      </div>
                      <div>
                        <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Day (1=Sun-7=Sat)</label>
                        <input
                          type="number"
                          min="1"
                          max="7"
                          value={meterSettings.timezone.stdDow || ''}
                          onChange={(e) => setMeterSettings({
                            ...meterSettings,
                            timezone: { ...meterSettings.timezone, stdDow: parseInt(e.target.value) }
                          })}
                          className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        />
                      </div>
                      <div>
                        <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Month (1-12)</label>
                        <input
                          type="number"
                          min="1"
                          max="12"
                          value={meterSettings.timezone.stdMonth || ''}
                          onChange={(e) => setMeterSettings({
                            ...meterSettings,
                            timezone: { ...meterSettings.timezone, stdMonth: parseInt(e.target.value) }
                          })}
                          className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        />
                      </div>
                      <div>
                        <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Hour (0-23)</label>
                        <input
                          type="number"
                          min="0"
                          max="23"
                          value={meterSettings.timezone.stdHour || ''}
                          onChange={(e) => setMeterSettings({
                            ...meterSettings,
                            timezone: { ...meterSettings.timezone, stdHour: parseInt(e.target.value) }
                          })}
                          className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        />
                      </div>
                      <div>
                        <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">UTC Offset (min)</label>
                        <input
                          type="number"
                          value={meterSettings.timezone.stdOffset || ''}
                          onChange={(e) => setMeterSettings({
                            ...meterSettings,
                            timezone: { ...meterSettings.timezone, stdOffset: parseInt(e.target.value) }
                          })}
                          className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        />
                      </div>
                    </div>
                  </div>
                </>
              )}

              <button
                onClick={saveMeterSettings}
                disabled={loading}
                className="inline-flex items-center px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg shadow transition-colors disabled:opacity-50"
              >
                <Save className="h-5 w-5 mr-2" />
                Save Timezone Settings
              </button>
            </div>
          </div>
        )}

        {/* Data Logging Section */}
        {!loading && activeSection === 'logging' && meterSettings?.dataLogging && (
          <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
            <h2 className="text-xl font-semibold text-gray-900 dark:text-white mb-4">Data Logging Settings</h2>
            <div className="space-y-6">
              <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Logging Interval (ms)</label>
                  <input
                    type="number"
                    value={meterSettings.dataLogging.loggingInterval || ''}
                    onChange={(e) => setMeterSettings({
                      ...meterSettings,
                      dataLogging: { ...meterSettings.dataLogging, loggingInterval: parseInt(e.target.value) }
                    })}
                    className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Buffer Size</label>
                  <input
                    type="number"
                    value={meterSettings.dataLogging.bufferSize || ''}
                    onChange={(e) => setMeterSettings({
                      ...meterSettings,
                      dataLogging: { ...meterSettings.dataLogging, bufferSize: parseInt(e.target.value) }
                    })}
                    className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Power Loss Threshold</label>
                  <input
                    type="number"
                    step="0.1"
                    value={meterSettings.dataLogging.powerLossThreshold || ''}
                    onChange={(e) => setMeterSettings({
                      ...meterSettings,
                      dataLogging: { ...meterSettings.dataLogging, powerLossThreshold: parseFloat(e.target.value) }
                    })}
                    className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                  />
                </div>
              </div>

              <label className="flex items-center space-x-2">
                <input
                  type="checkbox"
                  checked={meterSettings.dataLogging.enablePowerLossDetection || false}
                  onChange={(e) => setMeterSettings({
                    ...meterSettings,
                    dataLogging: { ...meterSettings.dataLogging, enablePowerLossDetection: e.target.checked }
                  })}
                  className="w-4 h-4 text-blue-600 rounded"
                />
                <span className="text-sm text-gray-700 dark:text-gray-300">Enable Power Loss Detection</span>
              </label>

              <div>
                <div className="flex items-center justify-between mb-3">
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300">Log Fields</label>
                  <button
                    onClick={addLogField}
                    className="inline-flex items-center px-3 py-1 bg-green-600 hover:bg-green-700 text-white text-sm rounded transition-colors"
                  >
                    <Plus className="h-4 w-4 mr-1" />
                    Add Field
                  </button>
                </div>
                <div className="space-y-2">
                  {logFields.map((field, index) => (
                    <div key={index} className="flex items-center space-x-2">
                      <select
                        value={field}
                        onChange={(e) => updateLogField(index, e.target.value)}
                        className="flex-1 px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                      >
                        {availableRegisters.map(reg => (
                          <option key={reg.name} value={reg.name}>
                            {reg.friendlyName} ({reg.name}) - {reg.unit}
                          </option>
                        ))}
                      </select>
                      <button
                        onClick={() => removeLogField(index)}
                        className="p-2 text-red-600 hover:bg-red-100 dark:hover:bg-red-900 rounded transition-colors"
                      >
                        <X className="h-5 w-5" />
                      </button>
                    </div>
                  ))}
                </div>
              </div>

              <button
                onClick={saveMeterSettings}
                disabled={loading}
                className="inline-flex items-center px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg shadow transition-colors disabled:opacity-50"
              >
                <Save className="h-5 w-5 mr-2" />
                Save Logging Settings
              </button>
            </div>
          </div>
        )}

        {/* Display Section */}
        {!loading && activeSection === 'display' && meterSettings?.display && (
          <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
            <h2 className="text-xl font-semibold text-gray-900 dark:text-white mb-4">Display Settings</h2>
            <div className="space-y-6">
              <div>
                <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-3">LCD Display Fields</h3>
                <div className="space-y-4">
                  {['field0', 'field1', 'field2'].map((fieldKey, index) => (
                    <div key={fieldKey}>
                      <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                        Row {index + 1}
                      </label>
                      <select
                        value={meterSettings.display[fieldKey] || ''}
                        onChange={(e) => setMeterSettings({
                          ...meterSettings,
                          display: { ...meterSettings.display, [fieldKey]: e.target.value }
                        })}
                        className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                      >
                        {availableRegisters.map(reg => (
                          <option key={reg.name} value={reg.name}>
                            {reg.friendlyName} ({reg.name}) - {reg.unit}
                          </option>
                        ))}
                      </select>
                    </div>
                  ))}
                </div>
              </div>

              <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Backlight Timeout (ms)</label>
                  <input
                    type="number"
                    value={meterSettings.display.backlightTimeout || ''}
                    onChange={(e) => setMeterSettings({
                      ...meterSettings,
                      display: { ...meterSettings.display, backlightTimeout: parseInt(e.target.value) }
                    })}
                    className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Long Press Time (ms)</label>
                  <input
                    type="number"
                    value={meterSettings.display.longPressTime || ''}
                    onChange={(e) => setMeterSettings({
                      ...meterSettings,
                      display: { ...meterSettings.display, longPressTime: parseInt(e.target.value) }
                    })}
                    className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                  />
                </div>
              </div>

              <button
                onClick={saveMeterSettings}
                disabled={loading}
                className="inline-flex items-center px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg shadow transition-colors disabled:opacity-50"
              >
                <Save className="h-5 w-5 mr-2" />
                Save Display Settings
              </button>
            </div>
          </div>
        )}

        {/* System Section */}
        {!loading && activeSection === 'system' && meterSettings?.system && (
          <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
            <h2 className="text-xl font-semibold text-gray-900 dark:text-white mb-4">System Settings</h2>
            <div className="space-y-6">
              <label className="flex items-center space-x-2">
                <input
                  type="checkbox"
                  checked={meterSettings.system.autoRebootEnabled || false}
                  onChange={(e) => setMeterSettings({
                    ...meterSettings,
                    system: { ...meterSettings.system, autoRebootEnabled: e.target.checked }
                  })}
                  className="w-4 h-4 text-blue-600 rounded"
                />
                <span className="text-sm text-gray-700 dark:text-gray-300">Auto Reboot Enabled</span>
              </label>

              <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Reboot Interval (hours)</label>
                  <input
                    type="number"
                    value={meterSettings.system.rebootIntervalHours || ''}
                    onChange={(e) => setMeterSettings({
                      ...meterSettings,
                      system: { ...meterSettings.system, rebootIntervalHours: parseInt(e.target.value) }
                    })}
                    className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Reboot Hour (0-23)</label>
                  <input
                    type="number"
                    min="0"
                    max="23"
                    value={meterSettings.system.rebootHour || ''}
                    onChange={(e) => setMeterSettings({
                      ...meterSettings,
                      system: { ...meterSettings.system, rebootHour: parseInt(e.target.value) }
                    })}
                    className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                  />
                </div>
              </div>

              <button
                onClick={saveMeterSettings}
                disabled={loading}
                className="inline-flex items-center px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg shadow transition-colors disabled:opacity-50"
              >
                <Save className="h-5 w-5 mr-2" />
                Save System Settings
              </button>
            </div>
          </div>
        )}

        {/* Calibration Settings Section */}
        {!loading && activeSection === 'calibration' && calibrationSettings && (
          <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
            <h2 className="text-xl font-semibold text-gray-900 dark:text-white mb-4">Calibration Settings</h2>
            <div className="space-y-6">
              {/* Voltage Gains */}
              <div>
                <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-3">Voltage Gains (Hex)</h3>
                <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                  {['A', 'B', 'C'].map(phase => (
                    <div key={phase}>
                      <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Phase {phase}</label>
                      <input
                        type="text"
                        value={calibrationSettings[`ugain${phase}`] || ''}
                        onChange={(e) => setCalibrationSettings({ ...calibrationSettings, [`ugain${phase}`]: e.target.value })}
                        className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white font-mono"
                        placeholder="8000"
                      />
                    </div>
                  ))}
                </div>
              </div>

              {/* Current Gains */}
              <div>
                <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-3">Current Gains (Hex)</h3>
                <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                  {['A', 'B', 'C'].map(phase => (
                    <div key={phase}>
                      <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Phase {phase}</label>
                      <input
                        type="text"
                        value={calibrationSettings[`igain${phase}`] || ''}
                        onChange={(e) => setCalibrationSettings({ ...calibrationSettings, [`igain${phase}`]: e.target.value })}
                        className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white font-mono"
                        placeholder="8000"
                      />
                    </div>
                  ))}
                </div>
              </div>

              {/* Voltage Offsets */}
              <div>
                <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-3">Voltage Offsets (Hex)</h3>
                <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                  {['A', 'B', 'C'].map(phase => (
                    <div key={phase}>
                      <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Phase {phase}</label>
                      <input
                        type="text"
                        value={calibrationSettings[`uoffset${phase}`] || ''}
                        onChange={(e) => setCalibrationSettings({ ...calibrationSettings, [`uoffset${phase}`]: e.target.value })}
                        className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white font-mono"
                        placeholder="0"
                      />
                    </div>
                  ))}
                </div>
              </div>

              {/* Current Offsets */}
              <div>
                <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-3">Current Offsets (Hex)</h3>
                <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                  {['A', 'B', 'C'].map(phase => (
                    <div key={phase}>
                      <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Phase {phase}</label>
                      <input
                        type="text"
                        value={calibrationSettings[`ioffset${phase}`] || ''}
                        onChange={(e) => setCalibrationSettings({ ...calibrationSettings, [`ioffset${phase}`]: e.target.value })}
                        className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white font-mono"
                        placeholder="0"
                      />
                    </div>
                  ))}
                </div>
              </div>

              {/* Power Offsets */}
              <div>
                <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-3">Power Offsets</h3>
                <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                  {['A', 'B', 'C'].map(phase => (
                    <div key={phase}>
                      <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Phase {phase}</label>
                      <input
                        type="number"
                        value={calibrationSettings[`poffset${phase}`] || ''}
                        onChange={(e) => setCalibrationSettings({ ...calibrationSettings, [`poffset${phase}`]: parseInt(e.target.value) || 0 })}
                        className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        placeholder="0"
                      />
                    </div>
                  ))}
                </div>
              </div>

              {/* Reactive Power Offsets */}
              <div>
                <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-3">Reactive Power Offsets</h3>
                <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                  {['A', 'B', 'C'].map(phase => (
                    <div key={phase}>
                      <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Phase {phase}</label>
                      <input
                        type="number"
                        value={calibrationSettings[`qoffset${phase}`] || ''}
                        onChange={(e) => setCalibrationSettings({ ...calibrationSettings, [`qoffset${phase}`]: parseInt(e.target.value) || 0 })}
                        className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        placeholder="0"
                      />
                    </div>
                  ))}
                </div>
              </div>

              {/* Frequency Settings */}
              <div>
                <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-3">Frequency Settings</h3>
                <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                  <div>
                    <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Nominal Frequency (Hz)</label>
                    <input
                      type="number"
                      value={calibrationSettings.f0 || ''}
                      onChange={(e) => setCalibrationSettings({ ...calibrationSettings, f0: parseInt(e.target.value) || 60 })}
                      className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                      placeholder="60"
                    />
                  </div>
                  <div className="flex items-end">
                    <label className="flex items-center space-x-2">
                      <input
                        type="checkbox"
                        checked={calibrationSettings.freq60Hz || false}
                        onChange={(e) => setCalibrationSettings({ ...calibrationSettings, freq60Hz: e.target.checked })}
                        className="w-4 h-4 text-blue-600 rounded"
                      />
                      <span className="text-sm text-gray-700 dark:text-gray-300">60Hz Grid (uncheck for 50Hz)</span>
                    </label>
                  </div>
                </div>
              </div>

              <div className="flex space-x-3">
                <button
                  onClick={() => saveCalibration(false)}
                  disabled={loading}
                  className="inline-flex items-center px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg shadow transition-colors disabled:opacity-50"
                >
                  <Save className="h-5 w-5 mr-2" />
                  Save to Memory
                </button>
                <button
                  onClick={() => saveCalibration(true)}
                  disabled={loading}
                  className="inline-flex items-center px-4 py-2 bg-green-600 hover:bg-green-700 text-white rounded-lg shadow transition-colors disabled:opacity-50"
                >
                  <Zap className="h-5 w-5 mr-2" />
                  Save & Apply to Chip
                </button>
              </div>
            </div>
          </div>
        )}

        {/* Auto Calibration Section */}
        {!loading && activeSection === 'auto-cal' && (
          <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
            <h2 className="text-xl font-semibold text-gray-900 dark:text-white mb-4">
              <Zap className="inline h-6 w-6 mr-2" />
              Auto Calibration
            </h2>
            <p className="text-sm text-gray-600 dark:text-gray-400 mb-6">
              Automatically calculate calibration values based on known reference measurements.
            </p>
            
            <div className="space-y-4">
              <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Phase</label>
                  <select
                    value={autoCalForm.phase}
                    onChange={(e) => setAutoCalForm({ ...autoCalForm, phase: e.target.value })}
                    className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                  >
                    <option value="A">Phase A</option>
                    <option value="B">Phase B</option>
                    <option value="C">Phase C</option>
                  </select>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">Type</label>
                  <select
                    value={autoCalForm.type}
                    onChange={(e) => setAutoCalForm({ ...autoCalForm, type: e.target.value })}
                    className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                  >
                    <option value="voltage">Voltage</option>
                    <option value="current">Current</option>
                  </select>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                    Expected Value ({autoCalForm.type === 'voltage' ? 'V' : 'A'})
                  </label>
                  <input
                    type="number"
                    step="0.01"
                    value={autoCalForm.expected}
                    onChange={(e) => setAutoCalForm({ ...autoCalForm, expected: e.target.value })}
                    placeholder="120.0"
                    className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                  />
                  <p className="text-xs text-gray-500 dark:text-gray-400 mt-1">
                    Reference measurement from calibrated device
                  </p>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                    Measured Value ({autoCalForm.type === 'voltage' ? 'V' : 'A'})
                  </label>
                  <input
                    type="number"
                    step="0.01"
                    value={autoCalForm.measured}
                    onChange={(e) => setAutoCalForm({ ...autoCalForm, measured: e.target.value })}
                    placeholder="115.2"
                    className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                  />
                  <p className="text-xs text-gray-500 dark:text-gray-400 mt-1">
                    Current reading from this meter
                  </p>
                </div>
              </div>

              <button
                onClick={autoCalibrate}
                disabled={loading || !autoCalForm.expected || !autoCalForm.measured}
                className="inline-flex items-center px-4 py-2 bg-purple-600 hover:bg-purple-700 text-white rounded-lg shadow transition-colors disabled:opacity-50"
              >
                <Zap className="h-5 w-5 mr-2" />
                Calculate & Apply Calibration
              </button>
            </div>

            {/* Energy Calibration Section */}
            <div className="mt-8 pt-8 border-t border-gray-200 dark:border-gray-700">
              <h3 className="text-lg font-semibold text-gray-900 dark:text-white mb-4">
                Energy Calibration
              </h3>
              <p className="text-sm text-gray-600 dark:text-gray-400 mb-6">
                Calibrate energy accumulation using a known load. The ESP32 will calculate the required register values.
              </p>

              <div className="space-y-4">
                {/* Phase Selection */}
                <div>
                  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                    Select Phases to Calibrate
                  </label>
                  <div className="flex items-center space-x-6">
                    <label className="flex items-center">
                      <input
                        type="checkbox"
                        checked={energyCalForm.phaseA}
                        onChange={(e) => setEnergyCalForm({ ...energyCalForm, phaseA: e.target.checked })}
                        disabled={energyCalForm.calibrationStarted}
                        className="w-4 h-4 text-blue-600 bg-gray-100 border-gray-300 rounded focus:ring-blue-500 dark:focus:ring-blue-600 dark:ring-offset-gray-800 focus:ring-2 dark:bg-gray-700 dark:border-gray-600"
                      />
                      <span className="ml-2 text-sm text-gray-700 dark:text-gray-300">Phase A</span>
                    </label>
                    <label className="flex items-center">
                      <input
                        type="checkbox"
                        checked={energyCalForm.phaseB}
                        onChange={(e) => setEnergyCalForm({ ...energyCalForm, phaseB: e.target.checked })}
                        disabled={energyCalForm.calibrationStarted}
                        className="w-4 h-4 text-blue-600 bg-gray-100 border-gray-300 rounded focus:ring-blue-500 dark:focus:ring-blue-600 dark:ring-offset-gray-800 focus:ring-2 dark:bg-gray-700 dark:border-gray-600"
                      />
                      <span className="ml-2 text-sm text-gray-700 dark:text-gray-300">Phase B</span>
                    </label>
                    <label className="flex items-center">
                      <input
                        type="checkbox"
                        checked={energyCalForm.phaseC}
                        onChange={(e) => setEnergyCalForm({ ...energyCalForm, phaseC: e.target.checked })}
                        disabled={energyCalForm.calibrationStarted}
                        className="w-4 h-4 text-blue-600 bg-gray-100 border-gray-300 rounded focus:ring-blue-500 dark:focus:ring-blue-600 dark:ring-offset-gray-800 focus:ring-2 dark:bg-gray-700 dark:border-gray-600"
                      />
                      <span className="ml-2 text-sm text-gray-700 dark:text-gray-300">Phase C</span>
                    </label>
                  </div>
                </div>

                {/* Start Calibration Button */}
                {!energyCalForm.calibrationStarted && (
                  <button
                    onClick={startEnergyCalibration}
                    disabled={loading || (!energyCalForm.phaseA && !energyCalForm.phaseB && !energyCalForm.phaseC)}
                    className="inline-flex items-center px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg shadow transition-colors disabled:opacity-50"
                  >
                    <Zap className="h-5 w-5 mr-2" />
                    Start Calibration
                  </button>
                )}

                {/* Calibration Input Fields - shown after start */}
                {energyCalForm.calibrationStarted && (
                  <div className="mt-6 p-4 bg-blue-50 dark:bg-blue-900 rounded-lg">
                    <p className="text-sm text-blue-800 dark:text-blue-200 mb-4">
                      Calibration started! Connect your known load now and enter the values below:
                    </p>
                    <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                      <div>
                        <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                          Load (Watts)
                        </label>
                        <input
                          type="number"
                          step="0.1"
                          value={energyCalForm.loadWatts}
                          onChange={(e) => setEnergyCalForm({ ...energyCalForm, loadWatts: e.target.value })}
                          placeholder="1000"
                          className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        />
                        <p className="text-xs text-gray-500 dark:text-gray-400 mt-1">
                          Power consumption of known load
                        </p>
                      </div>
                      <div>
                        <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                          Duration (Minutes)
                        </label>
                        <input
                          type="number"
                          step="0.1"
                          value={energyCalForm.durationMinutes}
                          onChange={(e) => setEnergyCalForm({ ...energyCalForm, durationMinutes: e.target.value })}
                          placeholder="5"
                          className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white"
                        />
                        <p className="text-xs text-gray-500 dark:text-gray-400 mt-1">
                          How long the load ran
                        </p>
                      </div>
                    </div>

                    {/* Complete buttons for each phase */}
                    <div className="mt-4 flex flex-wrap gap-2">
                      {energyCalForm.phaseA && (
                        <button
                          onClick={() => completeEnergyCalibration('A')}
                          disabled={loading || !energyCalForm.loadWatts || !energyCalForm.durationMinutes}
                          className="inline-flex items-center px-4 py-2 bg-green-600 hover:bg-green-700 text-white rounded-lg shadow transition-colors disabled:opacity-50"
                        >
                          <Zap className="h-5 w-5 mr-2" />
                          Complete Phase A
                        </button>
                      )}
                      {energyCalForm.phaseB && (
                        <button
                          onClick={() => completeEnergyCalibration('B')}
                          disabled={loading || !energyCalForm.loadWatts || !energyCalForm.durationMinutes}
                          className="inline-flex items-center px-4 py-2 bg-green-600 hover:bg-green-700 text-white rounded-lg shadow transition-colors disabled:opacity-50"
                        >
                          <Zap className="h-5 w-5 mr-2" />
                          Complete Phase B
                        </button>
                      )}
                      {energyCalForm.phaseC && (
                        <button
                          onClick={() => completeEnergyCalibration('C')}
                          disabled={loading || !energyCalForm.loadWatts || !energyCalForm.durationMinutes}
                          className="inline-flex items-center px-4 py-2 bg-green-600 hover:bg-green-700 text-white rounded-lg shadow transition-colors disabled:opacity-50"
                        >
                          <Zap className="h-5 w-5 mr-2" />
                          Complete Phase C
                        </button>
                      )}
                    </div>
                  </div>
                )}
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default MeterConfigView;