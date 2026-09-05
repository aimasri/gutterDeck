import sys
from PyQt5.QtWidgets import QApplication, QWidget
from PyQt5.QtCore import QTimer
from config import load_config, save_config
from gutter_ui import GutterWidget
from x11_engine import X11Engine

class GutterDeckApp:
    def __init__(self):
        self.app = QApplication(sys.argv)
        self.app.setQuitOnLastWindowClosed(False) # CRITICAL: Prevents QDialogs from shutting down the app when closed!
        self.app.aboutToQuit.connect(self.cleanup_windows) # Hook to clean up managed windows on exit
        
        self.config = load_config()
        self.engine = X11Engine()
        self.gutters = []
        self.active_gutter_id = None
        
        # Debounce timer for smooth hover transitions between gutters
        self.hover_timer = QTimer()
        self.hover_timer.setSingleShot(True)
        self.hover_timer.timeout.connect(self.apply_hover_geometry)
        
        self.gutter_width = self.config['settings'].get('gutter_width', 40)
        self.screen_width = self.config['settings'].get('screen_width', 1080)
        self.screen_height = self.config['settings'].get('screen_height', 1920)
        
        # Create a single dummy window to act as the sole Taskbar icon
        self.master_taskbar_icon = QWidget()
        self.master_taskbar_icon.setWindowTitle("Gutter Deck")
        self.master_taskbar_icon.setGeometry(-5000, -5000, 1, 1)
        self.master_taskbar_icon.show()
        
        self.init_gutters()

    def cleanup_windows(self):
        """Called automatically when the application is quitting to close all managed windows."""
        print("Quitting Gutter Deck... cleaning up windows.")
        import subprocess
        for gutter in self.gutters:
            if getattr(gutter, 'window_id', None):
                subprocess.call(['xdotool', 'windowclose', str(gutter.window_id)])

    def init_gutters(self):
        print("Launching and binding applications...")
        
        # Prevent "Invisible App" lockout if the user deleted their last gutter
        if not self.config.get('gutters'):
            import uuid
            default_id = "gutter_" + str(uuid.uuid4())[:8]
            self.config['gutters'] = [{
                "id": default_id,
                "name": "Default Deck",
                "color": "#80225588",
                "command": "google-chrome --profile-directory='Default' --new-window"
            }]
            from config import save_config
            save_config(self.config)
            
        for g_conf in self.config.get('gutters', []):
            gutter = GutterWidget(g_conf, self.engine)
            gutter.clicked.connect(self.on_gutter_clicked)
            gutter.config_changed.connect(self.save_current_config)
            gutter.close_requested.connect(self.close_deck)
            gutter.add_requested.connect(self.on_add_requested)
            gutter.hover_changed.connect(self.update_gutter_geometry)
            gutter.split_requested.connect(self.on_split_requested)
            gutter.reorder_requested.connect(self.on_reorder_requested)
            self.gutters.append(gutter)
            
            cmd = g_conf.get('command')
            if cmd:
                print(f"Launching {g_conf.get('name')}...")
                hex_id = self.engine.launch_and_capture(cmd)
                if hex_id:
                    # Save the true Hex ID to the gutter
                    gutter.window_id = hex_id
                else:
                    print(f"Failed to capture Window ID for {g_conf.get('name')}")
                    
        # 1. PURGE MAXIMIZED STATES GLOBALLY ON BOOT
        hex_ids_to_purge = [g.window_id for g in self.gutters if g.window_id]
        if hex_ids_to_purge:
            self.engine.purge_maximized_state(hex_ids_to_purge)
            
        # Apply initial layout (focus the first gutter)
        if self.gutters:
            self.apply_accordion_layout(self.gutters[0].config['id'])

    def on_gutter_clicked(self, gutter_id):
        self.apply_accordion_layout(gutter_id)

    def on_add_requested(self, gutter_id, direction):
        import uuid
        from PyQt5.QtWidgets import QInputDialog
        from PyQt5.QtCore import Qt
        
        dialog = QInputDialog()
        dialog.setWindowFlags(dialog.windowFlags() | Qt.WindowStaysOnTopHint)
        dialog.setWindowTitle("Add Deck")
        dialog.setStyleSheet("""
            QDialog { background-color: #2b2b2b; color: white; }
            QLabel { color: white; font-family: Arial; font-size: 10pt; }
            QLineEdit { background-color: #1e1e1e; color: white; border: 1px solid #555; padding: 5px; font-family: monospace; }
            QPushButton { background-color: #555; color: white; border-radius: 4px; padding: 6px 12px; font-weight: bold; }
            QPushButton:hover { border: 1px solid white; }
        """)
        dialog.setLabelText("Enter the terminal command to launch this application:")
        dialog.setTextValue("chromium --incognito")
        dialog.resize(400, 150)
        
        if dialog.exec_() == QInputDialog.Accepted:
            command = dialog.textValue().strip()
            if not command:
                return
        else:
            return
            
        idx = 0
        for i, g in enumerate(self.gutters):
            if g.config['id'] == gutter_id:
                idx = i
                break
                
        insert_idx = idx if direction == "left" else idx + 1
        new_id = "gutter_" + str(uuid.uuid4())[:8]
        new_conf = {
            "id": new_id,
            "name": "New Deck",
            "color": "#80225588",
            "command": command
        }
        
        self.config['gutters'].insert(insert_idx, new_conf)
        
        gutter = GutterWidget(new_conf, self.engine)
        gutter.clicked.connect(self.on_gutter_clicked)
        gutter.config_changed.connect(self.save_current_config)
        gutter.close_requested.connect(self.close_deck)
        gutter.add_requested.connect(self.on_add_requested)
        gutter.hover_changed.connect(self.update_gutter_geometry)
        gutter.split_requested.connect(self.on_split_requested)
        gutter.reorder_requested.connect(self.on_reorder_requested)
        
        self.gutters.insert(insert_idx, gutter)
        
        # Launch window
        win_id = self.engine.launch_and_capture(new_conf["command"])
        if win_id:
            gutter.window_id = win_id
            self.engine.purge_maximized_state([win_id])
            
        self.save_current_config()
        self.apply_accordion_layout(new_id)

    def on_split_requested(self, gutter_id):
        if self.active_gutter_id == gutter_id:
            return # Can't split with itself
        if not self.active_gutter_id:
            return
            
        active_idx = next((i for i, g in enumerate(self.gutters) if g.config['id'] == self.active_gutter_id), -1)
        clicked_idx = next((i for i, g in enumerate(self.gutters) if g.config['id'] == gutter_id), -1)
        
        # Enforce adjacency constraint!
        if abs(active_idx - clicked_idx) != 1:
            print("Split aborted: Target window must be directly adjacent (left or right) to the active window.")
            return
            
        self.is_split_mode = True
        
        # Physically match screen placement to array index (smaller index gets left half)
        if clicked_idx < active_idx:
            left_id = gutter_id
            right_id = self.active_gutter_id
        else:
            left_id = self.active_gutter_id
            right_id = gutter_id
        
        for g in self.gutters:
            if not g.window_id:
                continue
                
            if g.config['id'] == left_id:
                self.engine.show_window(g.window_id, 0, 0, 540, self.screen_height)
                self.engine.activate_window(g.window_id)
            elif g.config['id'] == right_id:
                self.engine.show_window(g.window_id, 540, 0, 540, self.screen_height)
                self.engine.activate_window(g.window_id)
            else:
                self.engine.hide_window(g.window_id)
                
        # Update the UI layout to sit around them
        self.update_gutter_geometry()

    def on_reorder_requested(self, source_id, target_id):
        if source_id == target_id:
            return
            
        src_idx = next(i for i, g in enumerate(self.gutters) if g.config['id'] == source_id)
        tgt_idx = next(i for i, g in enumerate(self.gutters) if g.config['id'] == target_id)
        
        g = self.gutters.pop(src_idx)
        c = self.config['gutters'].pop(src_idx)
        
        self.gutters.insert(tgt_idx, g)
        self.config['gutters'].insert(tgt_idx, c)
        
        self.save_current_config()
        
        # If we are in split mode, we should just redraw gutters. Else, redraw accordion.
        if getattr(self, 'is_split_mode', False):
            self.update_gutter_geometry()
        else:
            if self.active_gutter_id:
                self.apply_accordion_layout(self.active_gutter_id)
            else:
                self.update_gutter_geometry()

    def apply_accordion_layout(self, active_gutter_id=None):
        self.switch_deck(active_gutter_id)

    def switch_deck(self, active_gutter_id):
        """Handles heavy X11 window swapping."""
        if active_gutter_id == self.active_gutter_id and not getattr(self, 'is_split_mode', False):
            return # Ignore redundant clicks
            
        self.active_gutter_id = active_gutter_id
        self.is_split_mode = False
        
        active_idx = 0
        for i, g in enumerate(self.gutters):
            if g.config['id'] == active_gutter_id:
                active_idx = i
                break
                
        window_width = self.screen_width
        
        # 1. Swap X11 Windows
        for i, gutter in enumerate(self.gutters):
            if gutter.window_id:
                if i == active_idx:
                    self.engine.show_window(gutter.window_id, 0, 0, window_width, self.screen_height)
                    self.engine.activate_window(gutter.window_id)
                else:
                    self.engine.hide_window(gutter.window_id)
                    
        # 2. Update Gutter Geometry
        self.update_gutter_geometry()
        
        # 3. Force gutters above the newly activated window
        for gutter in self.gutters:
            gutter.raise_()
            try:
                import subprocess
                subprocess.call(['wmctrl', '-i', '-r', hex(int(gutter.winId())), '-b', 'add,above,skip_taskbar'])
            except Exception:
                pass

    def update_gutter_geometry(self):
        """Starts a 50ms debounce timer to prevent jitter when crossing between expanded gutters."""
        self.hover_timer.start(50)

    def apply_hover_geometry(self):
        """Calculates and applies dynamic auto-hide geometries to the Qt widgets."""
        if not self.gutters: return
        
        active_idx = 0
        for i, g in enumerate(self.gutters):
            if g.config['id'] == self.active_gutter_id:
                active_idx = i
                break
                
        # Group hover states: If one left gutter is hovered, expand all left gutters!
        left_hovered = any(getattr(self.gutters[i], 'hovered', False) for i in range(active_idx + 1))
        right_hovered = any(getattr(self.gutters[i], 'hovered', False) for i in range(active_idx + 1, len(self.gutters)))
                
        # Calculate dynamic widths
        widths = []
        for i, g in enumerate(self.gutters):
            if i <= active_idx:
                widths.append(self.gutter_width if left_hovered else 8)
            else:
                widths.append(self.gutter_width if right_hovered else 8)
                
        # Stack left gutters
        current_left_x = 0
        for i in range(active_idx + 1):
            g = self.gutters[i]
            g.setGeometry(current_left_x, 0, widths[i], self.screen_height)
            g.show()
            g.raise_() # Force openbox to render this above the activated Chrome window
            current_left_x += widths[i]
            
        # Stack right gutters
        current_right_x = self.screen_width
        for i in range(len(self.gutters) - 1, active_idx, -1):
            g = self.gutters[i]
            current_right_x -= widths[i]
            g.setGeometry(current_right_x, 0, widths[i], self.screen_height)
            g.show()
            g.raise_() # Force openbox to render this above the activated Chrome window

    def save_current_config(self):
        # Update config dictionary with current widget states
        new_gutters = []
        for g in self.gutters:
            new_gutters.append(g.config)
        self.config['gutters'] = new_gutters
        
        # Save to disk
        save_config(self.config)

    def close_deck(self, gutter_id):
        """Closes the X11 window and permanently removes the deck."""
        gutter_to_remove = None
        for g in self.gutters:
            if g.config['id'] == gutter_id:
                gutter_to_remove = g
                break
                
        if gutter_to_remove:
            if gutter_to_remove.window_id:
                import subprocess
                subprocess.call(['xdotool', 'windowclose', str(gutter_to_remove.window_id)])
            
            self.gutters.remove(gutter_to_remove)
            self.config['gutters'] = [c for c in self.config['gutters'] if c['id'] != gutter_id]
            gutter_to_remove.close()
            
            self.save_current_config()
            
            # If there are any gutters left, activate the first one
            if self.gutters:
                self.apply_accordion_layout(self.gutters[0].config['id'])
            else:
                QApplication.instance().quit()

    def run(self):
        sys.exit(self.app.exec_())

if __name__ == "__main__":
    app = GutterDeckApp()
    app.run()
