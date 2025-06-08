using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace NESLauncher
{
    public partial class Form1 : Form
    {
        public string configPath = "nes-config.txt";
        public string[] lines;

        public Form1()
        {
            InitializeComponent();
            this.Text = "NES++ Launcher";
            this.Icon = new Icon("logo.ico");
            muteToolStripMenuItem.Checked = false;
            fullscreenToolStripMenuItem.Checked = false;
            lines = File.ReadAllLines(configPath);

            for (int i = 0; i < lines.Length; i++)
            {
                switch (lines[i])
                {
                    case "mute=true":
                        muteToolStripMenuItem.Checked = true;
                        break;
                    case "fullscreen=true":
                        fullscreenToolStripMenuItem.Checked = true;
                        break;
                    case "logCPU=true":
                        logCPUInstructionsToolStripMenuItem.Checked = true;
                        break;
                }
            }

        }

        void assignConfiguration(string Property, string value)
        {
            for (int i = 0; i < lines.Length; i++)
            {
                if (lines[i].StartsWith(Property))
                {
                    lines[i] = Property + value;
                }
            }
            File.WriteAllLines(configPath, lines);
        }

        private void oToolStripMenuItem_Click(object sender, EventArgs e)
        {
        
        }

        private void selectROMToolStripMenuItem_Click(object sender, EventArgs e)
        {
            OpenFileDialog ofd = new OpenFileDialog();

            ofd.InitialDirectory = "C:\\";
            ofd.Multiselect = false;
            ofd.Filter = "NES ROMS(*.nes)|*.nes";
            ofd.Title = "Select ROM";

            if (ofd.ShowDialog() == DialogResult.OK)
            {
                string romPath = ofd.FileName;
                assignConfiguration("romPath=", romPath);
                System.Diagnostics.Process.Start("NES++.exe");
                this.Close();
            }

            
        }

        private void muteToolStripMenuItem_Click(object sender, EventArgs e)
        {
            muteToolStripMenuItem.Checked = !muteToolStripMenuItem.Checked;
            if (muteToolStripMenuItem.Checked)
            {
                assignConfiguration("mute=", "true");
            }
            else
            {
                assignConfiguration("mute=", "false");
            }

        }

        private void fullscreenToolStripMenuItem_Click(object sender, EventArgs e)
        {
            fullscreenToolStripMenuItem.Checked = !fullscreenToolStripMenuItem.Checked;
            if (fullscreenToolStripMenuItem.Checked)
            {
                assignConfiguration("fullscreen=", "true");
            }
            else
            {
                assignConfiguration("fullscreen=", "false");
            }
        }

        private void logCPUInstructionsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            logCPUInstructionsToolStripMenuItem.Checked  = !logCPUInstructionsToolStripMenuItem.Checked;
            if (logCPUInstructionsToolStripMenuItem.Checked)
            {
                MessageBox.Show("Warning! This has yet to be optimized and will likely cause performance issues");
                assignConfiguration("logCPU=", "true");
            }
            else
            {
                assignConfiguration("logCPU=", "false");
            }
        }
    }
}
