import subprocess
import time

class X11Engine:
    def __init__(self):
        pass

    def get_top_level_windows(self):
        """Returns a set of Hexadecimal Window IDs natively managed by Openbox."""
        try:
            output = subprocess.check_output(['wmctrl', '-l']).decode('utf-8')
            # Extract the first column (the Hex ID) from each line
            return set(line.split()[0] for line in output.strip().split('\n') if line)
        except Exception as e:
            print(f"Error getting windows: {e}")
            return set()

    def launch_and_capture(self, command):
        """
        Launches a command and polls for a brand new Top-Level window to appear.
        This completely prevents catching transient internal sub-windows.
        """
        old_windows = self.get_top_level_windows()
        
        subprocess.Popen(command, shell=True)
        
        # Poll up to 15 seconds (60 * 0.25s) for a new window to map to X11
        for _ in range(60):
            time.sleep(0.25)
            try:
                from PyQt5.QtWidgets import QApplication
                if QApplication.instance():
                    QApplication.instance().processEvents()
            except Exception:
                pass
            
            current_windows = self.get_top_level_windows()
            new_windows = current_windows - old_windows
            
            if new_windows:
                hex_id = list(new_windows)[0]
                print(f"Captured True Top-Level Window: {hex_id}")
                
                # Force the window to hide from the taskbar
                subprocess.run(f"wmctrl -i -r {hex_id} -b add,skip_taskbar", shell=True)
                
                return hex_id
                
        return None

    def purge_maximized_state(self, hex_ids):
        """Globally strips the maximized state from an array of window IDs and waits for OS resolution."""
        for hex_id in hex_ids:
            try:
                subprocess.call(['wmctrl', '-i', '-r', hex_id, '-b', 'remove,maximized_vert,maximized_horz'])
            except Exception:
                pass
                
        # CRITICAL: Openbox is asynchronous. It takes time to process the un-maximize client messages.
        print("Waiting 0.5s for Openbox to process un-maximize states globally...")
        time.sleep(0.5)

    def hide_window(self, hex_id):
        """Completely unmaps a window from the X11 server, rendering it invisible and un-clickable."""
        if not hex_id:
            return
        try:
            subprocess.call(['xdotool', 'windowunmap', str(int(hex_id, 16))])
        except Exception:
            pass

    def show_window(self, hex_id, x, y, width, height):
        """Maps a window back to the X11 server and violently locks its geometry into the viewport."""
        if not hex_id:
            return
        try:
            # Map it back to the screen
            subprocess.call(['xdotool', 'windowmap', str(int(hex_id, 16))])
            # Lock geometry
            subprocess.call(['wmctrl', '-i', '-r', hex_id, '-e', f"0,{x},{y},{width},{height}"])
        except Exception as e:
            print(f"Error showing window {hex_id}: {e}")

    def activate_window(self, hex_id):
        if not hex_id:
            return
        try:
            # We can use wmctrl to activate as well! -a activates it.
            subprocess.call(['wmctrl', '-i', '-a', hex_id])
        except Exception:
            pass
