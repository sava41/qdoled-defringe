
include(CSharpUtilities)

add_executable(qod
  App.config
  App.xaml
  App.xaml.cs
  MainWindow.xaml
  MainWindow.xaml.cs
  Injector.cs
  KeyboardUtils.cs
  MainViewModel.cs
  MonitorData.cs
)

csharp_set_xaml_cs_properties(
  App.xaml
  App.xaml.cs
  MainWindow.xaml
  MainWindow.xaml.cs 
)

set_property(SOURCE App.xaml PROPERTY VS_XAML_TYPE "ApplicationDefinition")

set_property(TARGET qod PROPERTY VS_PACKAGE_REFERENCES "WindowsDisplayAPI_1.3.0.13")

set_property(SOURCE icon.co PROPERTY VS_TOOL_OVERRIDE "EmbeddedResource")

target_include_directories(qod PRIVATE WindowsDisplayAPI)

target_link_libraries(qod PUBLIC CommonLib CSharpLib WindowsDisplayAPI)

set_target_properties(qod PROPERTIES
	WIN32_EXECUTABLE TRUE
    VS_DOTNET_REFERENCES "System;System.Core;System.Data;System.Drawing;System.Windows.Forms;Microsoft.CSharp"
    VS_GLOBAL_ROOTNAMESPACE QOD
)

set_property(TARGET qod PROPERTY VS_DOTNET_REFERENCES
"System"
"System.Configuration" 
"System.Configuration.Install" 
"System.Data" 
"System.Drawing" 
"System.Management" 
"System.Security" 
"System.Transactions" 
"System.Windows.Forms"
"System.Web" 
"System.Xml" 
"System.Core" 
"System.Xaml"
"System.Xml.Linq" 
"System.Data.DataSetExtensions" 
"PresentationCore" 
"PresentationFramework" 
"Microsoft.CSharp" 
"Microsoft.Office.Interop.Excel"
"WindowsBase" 
)