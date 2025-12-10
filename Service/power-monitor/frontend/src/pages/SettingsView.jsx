import React, { useState, useEffect } from 'react';
import { Save, Trash2, Plus, Edit2, Check, X, AlertCircle, Settings as SettingsIcon } from 'lucide-react';
import { useNavigate } from 'react-router-dom';

const SettingsView = ({ meters, onMetersUpdate }) => {
  const navigate = useNavigate();
  const [systemSettings, setSystemSettings] = useState({
    poll_interval: 1.0,
    energy_poll_interval: 30.0,
    data_retention_days: null,
    max_websocket_clients: 100
  });
  const [editingMeter, setEditingMeter] = useState(null);
  const [newMeter, setNewMeter] = useState({
    meter_id: '',
    ip_address: '',
    name: '',
    location: '',
    enabled: true,
    poll_interval: 1.0,
    energy_poll_interval: 30.0
  });
  const [showAddMeter, setShowAddMeter] = useState(false);
  const [saveMessage, setSaveMessage] = useState('');

  useEffect(() => {
    fetchSystemSettings();
  }, []);

  const fetchSystemSettings = async () => {
    try {
      const response = await fetch('/api/settings');
      const data = await response.json();
      setSystemSettings(data);
    } catch (error) {
      console.error('Error fetching settings:', error);
    }
  };

  const saveSystemSettings = async () => {
    try {
      await fetch('/api/settings', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(systemSettings)
      });
      showSaveMessage('System settings saved successfully!');
    } catch (error) {
      console.error('Error saving settings:', error);
      showSaveMessage('Error saving settings', true);
    }
  };

  const addMeter = async () => {
    try {
      await fetch('/api/meters/register', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(newMeter)
      });
      setShowAddMeter(false);
      setNewMeter({
        meter_id: '',
        ip_address: '',
        name: '',
        location: '',
        enabled: true,
        poll_interval: 1.0,
        energy_poll_interval: 30.0
      });
      onMetersUpdate();
      showSaveMessage('Meter added successfully!');
    } catch (error) {
      console.error('Error adding meter:', error);
      showSaveMessage('Error adding meter', true);
    }
  };

  const updateMeter = async (meterId, meterData) => {
    try {
      await fetch(`/api/meters/${meterId}`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(meterData)
      });
      setEditingMeter(null);
      onMetersUpdate();
      showSaveMessage('Meter updated successfully!');
    } catch (error) {
      console.error('Error updating meter:', error);
      showSaveMessage('Error updating meter', true);
    }
  };

  const deleteMeter = async (meterId) => {
    if (!window.confirm('Are you sure you want to delete this meter? All associated data will be removed.')) {
      return;
    }

    try {
      await fetch(`/api/meters/${meterId}`, {
        method: 'DELETE'
      });
      onMetersUpdate();
      showSaveMessage('Meter deleted successfully!');
    } catch (error) {
      console.error('Error deleting meter:', error);
      showSaveMessage('Error deleting meter', true);
    }
  };

  const showSaveMessage = (message, isError = false) => {
    setSaveMessage({ text: message, isError });
    setTimeout(() => setSaveMessage(''), 3000);
  };

  const openMeterConfig = (meter) => {
    navigate(`/settings/meter/${meter.meter_id}`);
  };

  const [healthStats, setHealthStats] = useState(null);
  
  useEffect(() => {
    const fetchHealth = async () => {
      try {
        const response = await fetch('/health/detailed');
        const data = await response.json();
        setHealthStats(data);
      } catch (error) {
        console.error('Error fetching health:', error);
      }
    };
    
    fetchHealth();
    const interval = setInterval(fetchHealth, 30000); // Update every 30s
    return () => clearInterval(interval);
  }, []);

  return (
    <div className="space-y-6">
      {/* Header */}
      <h1 className="text-3xl font-bold text-gray-900 dark:text-white">Settings</h1>

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
		
		

		{healthStats && (
		<div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
			<h2 className="text-xl font-semibold text-gray-900 dark:text-white mb-4">System Health</h2>
			<div className="grid grid-cols-2 md:grid-cols-4 gap-4">
			<div className="p-4 bg-gray-50 dark:bg-gray-700 rounded-lg">
				<p className="text-sm text-gray-600 dark:text-gray-400">Memory</p>
				<p className="text-2xl font-bold text-gray-900 dark:text-white">
				{healthStats.memory_mb}
				<span className="text-sm font-normal text-gray-600 dark:text-gray-400 ml-1">MB</span>
				</p>
			</div>
			{healthStats.database && (
				<>
				<div className="p-4 bg-gray-50 dark:bg-gray-700 rounded-lg">
					<p className="text-sm text-gray-600 dark:text-gray-400">Database</p>
					<p className="text-2xl font-bold text-gray-900 dark:text-white">
					{healthStats.database.total_size_mb}
					<span className="text-sm font-normal text-gray-600 dark:text-gray-400 ml-1">MB</span>
					</p>
				</div>
				<div className="p-4 bg-gray-50 dark:bg-gray-700 rounded-lg">
					<p className="text-sm text-gray-600 dark:text-gray-400">Disk Free</p>
					<p className={`text-2xl font-bold ${
					healthStats.database.disk_free_gb < 1 ? 'text-red-600' :
					healthStats.database.disk_free_gb < 2 ? 'text-yellow-600' :
					'text-gray-900 dark:text-white'
					}`}>
					{healthStats.database.disk_free_gb}
					<span className="text-sm font-normal text-gray-600 dark:text-gray-400 ml-1">GB</span>
					</p>
				</div>
				<div className="p-4 bg-gray-50 dark:bg-gray-700 rounded-lg">
					<p className="text-sm text-gray-600 dark:text-gray-400">Readings</p>
					<p className="text-2xl font-bold text-gray-900 dark:text-white">
					{healthStats.database.reading_count.toLocaleString()}
					</p>
				</div>
				</>
			)}
			</div>
		</div>
		)}	  

      {/* System Settings */}
      <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
        <h2 className="text-xl font-semibold text-gray-900 dark:text-white mb-4">System Settings</h2>
        <div className="space-y-4">
          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
              Default Poll Interval (seconds)
            </label>
            <input
              type="number"
              step="0.1"
              min="0.1"
              value={systemSettings.poll_interval}
              onChange={(e) => setSystemSettings({
                ...systemSettings,
                poll_interval: parseFloat(e.target.value)
              })}
              className="w-full md:w-64 px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
            />
            <p className="text-sm text-gray-500 dark:text-gray-400 mt-1">
              How often to poll meters for power readings (minimum 0.1 seconds)
            </p>
          </div>

          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
              Default Energy Poll Interval (seconds)
            </label>
            <input
              type="number"
              step="1"
              min="5"
              value={systemSettings.energy_poll_interval}
              onChange={(e) => setSystemSettings({
                ...systemSettings,
                energy_poll_interval: parseFloat(e.target.value)
              })}
              className="w-full md:w-64 px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
            />
            <p className="text-sm text-gray-500 dark:text-gray-400 mt-1">
              How often to poll meters for energy readings (minimum 5 seconds, recommended 30)
            </p>
          </div>

          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
              Data Retention (days)
            </label>
            <input
              type="number"
              min="1"
              value={systemSettings.data_retention_days || ''}
              onChange={(e) => setSystemSettings({
                ...systemSettings,
                data_retention_days: e.target.value ? parseInt(e.target.value) : null
              })}
              placeholder="Keep forever (leave empty)"
              className="w-full md:w-64 px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
            />
            <p className="text-sm text-gray-500 dark:text-gray-400 mt-1">
              Auto-delete data older than this (empty = keep forever)
            </p>
          </div>
		  
		  <div>
		  <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
			  Minimum Free Space (GB)
		  </label>
		  <input
			  type="number"
			  step="0.1"
			  min="0.5"
			  value={systemSettings.min_free_space_gb || 1.0}
			  onChange={(e) => setSystemSettings({
			  ...systemSettings,
			  min_free_space_gb: parseFloat(e.target.value)
			  })}
			  className="w-full md:w-64 px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
		  />
		  <p className="text-sm text-gray-500 dark:text-gray-400 mt-1">
			  Automatically delete oldest data when free space drops below this
		  </p>
		  </div>		  

          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
              Max WebSocket Clients
            </label>
            <input
              type="number"
              min="1"
              value={systemSettings.max_websocket_clients}
              onChange={(e) => setSystemSettings({
                ...systemSettings,
                max_websocket_clients: parseInt(e.target.value)
              })}
              className="w-full md:w-64 px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
            />
          </div>

          <button
            onClick={saveSystemSettings}
            className="inline-flex items-center px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg shadow transition-colors"
          >
            <Save className="h-5 w-5 mr-2" />
            Save System Settings
          </button>
        </div>
      </div>

      {/* Meters Management */}
      <div className="bg-white dark:bg-gray-800 rounded-lg shadow p-6">
        <div className="flex items-center justify-between mb-4">
          <h2 className="text-xl font-semibold text-gray-900 dark:text-white">Power Meters</h2>
          <button
            onClick={() => setShowAddMeter(!showAddMeter)}
            className="inline-flex items-center px-4 py-2 bg-green-600 hover:bg-green-700 text-white rounded-lg shadow transition-colors"
          >
            <Plus className="h-5 w-5 mr-2" />
            Add Meter
          </button>
        </div>

        {/* Add Meter Form */}
        {showAddMeter && (
          <div className="mb-6 p-4 bg-gray-50 dark:bg-gray-700 rounded-lg">
            <h3 className="text-lg font-medium text-gray-900 dark:text-white mb-4">Add New Meter</h3>
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              <div>
                <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                  Meter ID *
                </label>
                <input
                  type="text"
                  value={newMeter.meter_id}
                  onChange={(e) => setNewMeter({ ...newMeter, meter_id: e.target.value })}
                  className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-800 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
                  placeholder="meter_001"
                />
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                  IP Address *
                </label>
                <input
                  type="text"
                  value={newMeter.ip_address}
                  onChange={(e) => setNewMeter({ ...newMeter, ip_address: e.target.value })}
                  className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-800 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
                  placeholder="192.168.1.100"
                />
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                  Name
                </label>
                <input
                  type="text"
                  value={newMeter.name}
                  onChange={(e) => setNewMeter({ ...newMeter, name: e.target.value })}
                  className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-800 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
                  placeholder="Main Panel"
                />
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                  Location
                </label>
                <input
                  type="text"
                  value={newMeter.location}
                  onChange={(e) => setNewMeter({ ...newMeter, location: e.target.value })}
                  className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-800 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
                  placeholder="Garage"
                />
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                  Poll Interval (seconds)
                </label>
                <input
                  type="number"
                  step="0.1"
                  min="0.1"
                  value={newMeter.poll_interval}
                  onChange={(e) => setNewMeter({ ...newMeter, poll_interval: parseFloat(e.target.value) })}
                  className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-800 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
                />
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-2">
                  Energy Poll Interval (seconds)
                </label>
                <input
                  type="number"
                  step="1"
                  min="5"
                  value={newMeter.energy_poll_interval}
                  onChange={(e) => setNewMeter({ ...newMeter, energy_poll_interval: parseFloat(e.target.value) })}
                  className="w-full px-4 py-2 rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-800 text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500"
                />
              </div>
            </div>
            <div className="flex items-center space-x-3 mt-4">
              <button
                onClick={addMeter}
                disabled={!newMeter.meter_id || !newMeter.ip_address}
                className="inline-flex items-center px-4 py-2 bg-green-600 hover:bg-green-700 text-white rounded-lg shadow transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
              >
                <Check className="h-5 w-5 mr-2" />
                Add Meter
              </button>
              <button
                onClick={() => setShowAddMeter(false)}
                className="inline-flex items-center px-4 py-2 bg-gray-500 hover:bg-gray-600 text-white rounded-lg shadow transition-colors"
              >
                <X className="h-5 w-5 mr-2" />
                Cancel
              </button>
            </div>
          </div>
        )}

        {/* Meters List */}
        <div className="space-y-4">
          {meters.map(meter => (
            <MeterCard
              key={meter.meter_id}
              meter={meter}
              isEditing={editingMeter === meter.meter_id}
              onEdit={() => setEditingMeter(meter.meter_id)}
              onSave={(data) => updateMeter(meter.meter_id, data)}
              onCancel={() => setEditingMeter(null)}
              onDelete={() => deleteMeter(meter.meter_id)}
              onConfigure={openMeterConfig}
            />
          ))}
        </div>
      </div>
    </div>
  );
};

const MeterCard = ({ meter, isEditing, onEdit, onSave, onCancel, onDelete, onConfigure }) => {
  const [editedMeter, setEditedMeter] = useState(meter);

  useEffect(() => {
    setEditedMeter(meter);
  }, [meter]);

  if (isEditing) {
    return (
      <div className="p-4 bg-gray-50 dark:bg-gray-700 rounded-lg">
        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">
              IP Address
            </label>
            <input
              type="text"
              value={editedMeter.ip_address}
              onChange={(e) => setEditedMeter({ ...editedMeter, ip_address: e.target.value })}
              className="w-full px-3 py-2 rounded border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-800 text-gray-900 dark:text-white"
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">
              Name
            </label>
            <input
              type="text"
              value={editedMeter.name || ''}
              onChange={(e) => setEditedMeter({ ...editedMeter, name: e.target.value })}
              className="w-full px-3 py-2 rounded border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-800 text-gray-900 dark:text-white"
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">
              Location
            </label>
            <input
              type="text"
              value={editedMeter.location || ''}
              onChange={(e) => setEditedMeter({ ...editedMeter, location: e.target.value })}
              className="w-full px-3 py-2 rounded border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-800 text-gray-900 dark:text-white"
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">
              Poll Interval (s)
            </label>
            <input
              type="number"
              step="0.1"
              value={editedMeter.poll_interval}
              onChange={(e) => setEditedMeter({ ...editedMeter, poll_interval: parseFloat(e.target.value) })}
              className="w-full px-3 py-2 rounded border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-800 text-gray-900 dark:text-white"
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">
              Energy Poll Interval (s)
            </label>
            <input
              type="number"
              step="1"
              value={editedMeter.energy_poll_interval || 30.0}
              onChange={(e) => setEditedMeter({ ...editedMeter, energy_poll_interval: parseFloat(e.target.value) })}
              className="w-full px-3 py-2 rounded border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-800 text-gray-900 dark:text-white"
            />
          </div>
        </div>
        <div className="flex items-center space-x-3 mt-4">
          <button
            onClick={() => onSave(editedMeter)}
            className="inline-flex items-center px-3 py-1.5 bg-green-600 hover:bg-green-700 text-white rounded text-sm"
          >
            <Check className="h-4 w-4 mr-1" />
            Save
          </button>
          <button
            onClick={onCancel}
            className="inline-flex items-center px-3 py-1.5 bg-gray-500 hover:bg-gray-600 text-white rounded text-sm"
          >
            <X className="h-4 w-4 mr-1" />
            Cancel
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="p-4 bg-gray-50 dark:bg-gray-700 rounded-lg flex items-center justify-between">
      <div className="flex-1">
        <h3 className="text-lg font-medium text-gray-900 dark:text-white">
          {meter.name || meter.meter_id}
        </h3>
        <div className="mt-2 grid grid-cols-2 md:grid-cols-4 gap-4 text-sm">
          <div>
            <span className="text-gray-600 dark:text-gray-400">ID:</span>
            <span className="ml-2 text-gray-900 dark:text-white">{meter.meter_id}</span>
          </div>
          <div>
            <span className="text-gray-600 dark:text-gray-400">IP:</span>
            <span className="ml-2 text-gray-900 dark:text-white">{meter.ip_address}</span>
          </div>
          <div>
            <span className="text-gray-600 dark:text-gray-400">Location:</span>
            <span className="ml-2 text-gray-900 dark:text-white">{meter.location || 'N/A'}</span>
          </div>
          <div>
            <span className="text-gray-600 dark:text-gray-400">Poll:</span>
            <span className="ml-2 text-gray-900 dark:text-white">{meter.poll_interval}s</span>
          </div>
        </div>
      </div>
      <div className="flex items-center space-x-2 ml-4">
        <button
          onClick={onEdit}
          className="p-2 text-blue-600 dark:text-blue-400 hover:bg-blue-100 dark:hover:bg-blue-900 rounded transition-colors"
          aria-label="Edit meter"
        >
          <Edit2 className="h-5 w-5" />
        </button>
        <button
          onClick={() => onConfigure(meter)}
          className="p-2 text-purple-600 dark:text-purple-400 hover:bg-purple-100 dark:hover:bg-purple-900 rounded transition-colors"
          aria-label="Configure meter"
          title="Advanced Configuration"
        >
          <SettingsIcon className="h-5 w-5" />
        </button>
        <button
          onClick={onDelete}
          className="p-2 text-red-600 dark:text-red-400 hover:bg-red-100 dark:hover:bg-red-900 rounded transition-colors"
          aria-label="Delete meter"
        >
          <Trash2 className="h-5 w-5" />
        </button>
      </div>
    </div>
  );
};

export default SettingsView;