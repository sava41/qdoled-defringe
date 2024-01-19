using System.Collections;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Windows.Controls;
using System.Windows.Input;

namespace QOD
{
    public class MonitorData : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;
        public static event PropertyChangedEventHandler StaticPropertyChanged;

        private float _filterStrength;

        public MonitorData(string devicePath, uint sourceId, string name, string connector, string position, float filterStrength)
        {
            DevicePath = devicePath;
            SourceId = sourceId;
            Name = name;
            Connector = connector;
            Position = position;
            FilterStrength = filterStrength;
        }

        public MonitorData(string devicePath)
        {
            DevicePath = devicePath;
        }

        public string DevicePath { get; }
        public uint SourceId { get; }
        public string Name { get; }
        public string Connector { get; }
        public string Position { get; }
        public float FilterStrength
        {
            set
            {
                if (value == _filterStrength) return;
                _filterStrength = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(FilterStrength)));
                StaticPropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(FilterStrength)));
            }
            get => _filterStrength;
        }


    }

    public class RangeValidationRule : ValidationRule
    {
        public float Minimum { get; set; }
        public float Maximum { get; set; }

        public override ValidationResult Validate(object value, CultureInfo cultureInfo)
        {
            float number = 0;
            try
            {
                if (((string)value).Length > 0)
                    number = float.Parse((string)value);
            }
            catch
            {
                return new ValidationResult(false, "Not a valid number");
            }

            if ((number < Minimum) || (number > Maximum))
                return new ValidationResult(false, $"Enter a number in the range: {Minimum}-{Maximum}");

            return ValidationResult.ValidResult;
        }
    }
}