import React, { useState, useEffect } from 'react';
import { BrowserRouter as Router, Routes, Route, NavLink } from 'react-router-dom';
import { Activity, History, Settings as SettingsIcon, Moon, Sun } from 'lucide-react';
import LiveView from './pages/LiveView';
import HistoricalView from './pages/HistoricalView';
import SettingsView from './pages/SettingsView';
import MeterConfigView from './pages/MeterConfigView';
import './App.css';

function App() {
  const [darkMode, setDarkMode] = useState(true);
  const [meters, setMeters] = useState([]);

  useEffect(() => {
    // Load theme preference
    const savedTheme = localStorage.getItem('theme');
    if (savedTheme) {
      setDarkMode(savedTheme === 'dark');
    }
  }, []);

  useEffect(() => {
    // Apply theme
    if (darkMode) {
      document.documentElement.classList.add('dark');
    } else {
      document.documentElement.classList.remove('dark');
    }
    localStorage.setItem('theme', darkMode ? 'dark' : 'light');
  }, [darkMode]);

  useEffect(() => {
    // Fetch meters list
    fetchMeters();
  }, []);

  const fetchMeters = async () => {
    try {
      const response = await fetch('/api/meters');
      const data = await response.json();
      setMeters(data);
    } catch (error) {
      console.error('Error fetching meters:', error);
    }
  };

  const toggleTheme = () => {
    setDarkMode(!darkMode);
  };

  return (
    <Router>
      <div className="min-h-screen bg-gray-50 dark:bg-gray-900 transition-colors">
        {/* Navigation */}
        <nav className="bg-white dark:bg-gray-800 border-b border-gray-200 dark:border-gray-700 shadow-sm">
          <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
            <div className="flex justify-between h-16">
              <div className="flex">
                <div className="flex-shrink-0 flex items-center">
                  <Activity className="h-8 w-8 text-blue-600 dark:text-blue-400" />
                  <span className="ml-2 text-xl font-bold text-gray-900 dark:text-white">
                    Power Monitor
                  </span>
                </div>
                <div className="ml-10 flex space-x-4">
                  <NavLink
                    to="/"
                    className={({ isActive }) =>
                      `inline-flex items-center px-3 py-2 text-sm font-medium rounded-md transition-colors ${
                        isActive
                          ? 'bg-blue-100 dark:bg-blue-900 text-blue-700 dark:text-blue-200'
                          : 'text-gray-700 dark:text-gray-300 hover:bg-gray-100 dark:hover:bg-gray-700'
                      }`
                    }
                  >
                    <Activity className="h-5 w-5 mr-2" />
                    Live
                  </NavLink>
                  <NavLink
                    to="/historical"
                    className={({ isActive }) =>
                      `inline-flex items-center px-3 py-2 text-sm font-medium rounded-md transition-colors ${
                        isActive
                          ? 'bg-blue-100 dark:bg-blue-900 text-blue-700 dark:text-blue-200'
                          : 'text-gray-700 dark:text-gray-300 hover:bg-gray-100 dark:hover:bg-gray-700'
                      }`
                    }
                  >
                    <History className="h-5 w-5 mr-2" />
                    Historical
                  </NavLink>
                  <NavLink
                    to="/settings"
                    className={({ isActive }) =>
                      `inline-flex items-center px-3 py-2 text-sm font-medium rounded-md transition-colors ${
                        isActive
                          ? 'bg-blue-100 dark:bg-blue-900 text-blue-700 dark:text-blue-200'
                          : 'text-gray-700 dark:text-gray-300 hover:bg-gray-100 dark:hover:bg-gray-700'
                      }`
                    }
                  >
                    <SettingsIcon className="h-5 w-5 mr-2" />
                    Settings
                  </NavLink>
                </div>
              </div>
              <div className="flex items-center">
                <button
                  onClick={toggleTheme}
                  className="p-2 rounded-lg text-gray-700 dark:text-gray-300 hover:bg-gray-100 dark:hover:bg-gray-700 transition-colors"
                  aria-label="Toggle theme"
                >
                  {darkMode ? (
                    <Sun className="h-5 w-5" />
                  ) : (
                    <Moon className="h-5 w-5" />
                  )}
                </button>
              </div>
            </div>
          </div>
        </nav>

        {/* Main Content */}
        <main className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
          <Routes>
            <Route path="/" element={<LiveView meters={meters} />} />
            <Route path="/historical" element={<HistoricalView meters={meters} />} />
            <Route path="/settings" element={<SettingsView meters={meters} onMetersUpdate={fetchMeters} />} />
			<Route path="/settings/meter/:meterId" element={<MeterConfigView meters={meters} />} />
          </Routes>
        </main>
      </div>
    </Router>
  );
}

export default App;