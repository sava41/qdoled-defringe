
include(CSharpUtilities)

add_executable(qod
  App.config
  App.xaml
  App.xaml.cs
  MainWindow.xaml
  MainWindow.xaml.cs
  OverlayWindow.xaml
  OverlayWindow.xaml.cs
  
  Injector.cs
  KeyboardUtils.cs
  MainViewModel.cs
  MonitorData.cs

  icon.ico

  App.manifest
)

csharp_set_xaml_cs_properties(
  App.xaml
  App.xaml.cs
  MainWindow.xaml
  MainWindow.xaml.cs
  OverlayWindow.xaml
  OverlayWindow.xaml.cs
)

set_property(SOURCE App.xaml PROPERTY VS_XAML_TYPE "ApplicationDefinition")
set_property(SOURCE icon.ico PROPERTY VS_TOOL_OVERRIDE "EmbeddedResource")

set_property(TARGET qod PROPERTY VS_GLOBAL_Prefer32Bit "false")
set_property(TARGET qod PROPERTY VS_GLOBAL_ApplicationManifest "${CMAKE_CURRENT_SOURCE_DIR}/App.manifest")
set_property(TARGET qod PROPERTY VS_GLOBAL_ApplicationIcon "${CMAKE_CURRENT_SOURCE_DIR}/icon.ico")
set_property(TARGET qod PROPERTY VS_PACKAGE_REFERENCES "WindowsDisplayAPI_1.3.0.13")
set_property(TARGET qod PROPERTY WIN32_EXECUTABLE TRUE)
set_property(TARGET qod PROPERTY VS_GLOBAL_ROOTNAMESPACE QOD)
set_property(TARGET qod PROPERTY VS_DOTNET_REFERENCES
  "System"
  "System.Drawing" 
  "System.Windows.Forms"
  "System.Xml"
  "System.Core"
  "System.Xaml"
  "System.Xml.Linq"
  "PresentationCore"
  "PresentationFramework"
  "Microsoft.CSharp"
  "WindowsBase"
)