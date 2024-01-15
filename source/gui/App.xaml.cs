using System.Windows;
using System.Windows.Input;

namespace QOD
{
    public partial class App
    {
        public static KeyboardListener KListener = new KeyboardListener();

        private void App_OnExit(object sender, ExitEventArgs e)
        {
            KListener.Dispose();
        }
    }
}