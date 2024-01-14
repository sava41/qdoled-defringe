using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Windows.Input;
using System.Xml;
using System.Xml.Linq;

namespace QOD
{
    internal class MainViewModel : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;

        private string _activeText;
        private MonitorData _selectedMonitor;
        private bool _isActive;
        private Key _toggleKey;

        public MainViewModel()
        {
            UpdateActiveStatus();
            var dispatcherTimer = new System.Windows.Threading.DispatcherTimer();
            dispatcherTimer.Tick += DispatcherTimer_Tick;
            dispatcherTimer.Interval = new TimeSpan(0, 0, 1);
            dispatcherTimer.Start();

            _allMonitors = new List<MonitorData>();
            Monitors = new ObservableCollection<MonitorData>();
            UpdateMonitors();

            CanApply = !Injector.NoDebug;
        }

        public string ActiveText
        {
            private set
            {
                if (value == _activeText) return;
                _activeText = value;
                OnPropertyChanged();
            }
            get => _activeText;
        }

        public MonitorData SelectedMonitor
        {
            set
            {
                if (value == _selectedMonitor) return;
                _selectedMonitor = value;
                OnPropertyChanged();
            }
            get => _selectedMonitor;
        }

        public Key ToggleKey
        {
            set
            {
                if (value == _toggleKey) return;
                _toggleKey = value;
                OnPropertyChanged();
            }
            get => _toggleKey;
        }

        public bool IsActive
        {
            set
            {
                if (value == _isActive) return;
                _isActive = value;
                OnPropertyChanged();
            }
            get => _isActive;
        }

        public bool CanApply { get; }

        private List<MonitorData> _allMonitors { get; }
        public ObservableCollection<MonitorData> Monitors { get; }

        public void UpdateMonitors()
        {
            var selectedPath = SelectedMonitor?.DevicePath;
            _allMonitors.Clear();
            Monitors.Clear();
            var paths = WindowsDisplayAPI.DisplayConfig.PathInfo.GetActivePaths();
            foreach (var path in paths)
            {
                if (path.IsCloneMember) continue;
                var targetInfo = path.TargetsInfo[0];
                var deviceId = targetInfo.DisplayTarget.TargetId;
                var devicePath = targetInfo.DisplayTarget.DevicePath;
                var name = targetInfo.DisplayTarget.FriendlyName;
                if (string.IsNullOrEmpty(name))
                {
                    name = "???";
                }

                var connector = targetInfo.OutputTechnology.ToString();
                if (connector == "DisplayPortExternal")
                {
                    connector = "DisplayPort";
                }

                var position = path.Position.X + "," + path.Position.Y;

                var monitor = new MonitorData(devicePath, path.DisplaySource.SourceId + 1, name, connector, 1.0f);
                _allMonitors.Add(monitor);
                Monitors.Add(monitor);
            }

            if (selectedPath == null) return;

            var previous = Monitors.FirstOrDefault(monitor => monitor.DevicePath == selectedPath);
            if (previous != null)
            {
                SelectedMonitor = previous;
            }
        }

        public void ReInject()
        {
            Injector.Uninject();
            Injector.Inject(Monitors);

            UpdateActiveStatus();
        }

        public void Uninject()
        {
            Injector.Uninject();
            UpdateActiveStatus();
        }

        private void UpdateActiveStatus()
        {
            var status = Injector.GetStatus();
            if (status != null)
            {
                IsActive = (bool)status;
                if (status == true)
                {
                    ActiveText = "Active";
                }
                else
                {
                    ActiveText = "Inactive";
                }
            }
            else
            {
                IsActive = false;
                ActiveText = "???";
            }
        }

        public void OnDisplaySettingsChanged(object sender, EventArgs e)
        {
            UpdateMonitors();
            ReInject();
        }

        private void DispatcherTimer_Tick(object sender, EventArgs e)
        {
            UpdateActiveStatus();
        }

        private void OnPropertyChanged([CallerMemberName] string name = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
        }
    }
}