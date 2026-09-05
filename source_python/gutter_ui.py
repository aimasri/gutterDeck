import sys
from PyQt5.QtWidgets import QApplication, QWidget, QMenu, QInputDialog, QColorDialog, QDialog, QVBoxLayout, QHBoxLayout, QLabel, QLineEdit, QPushButton, QSlider, QGridLayout
from PyQt5.QtCore import Qt, pyqtSignal, QRectF, QTimer
from PyQt5.QtGui import QPainter, QColor, QFont, QBrush

class CustomInputDialog(QDialog):
    def __init__(self, title, label_text, default_text, accent_color, parent=None):
        super().__init__(None)
        self.setWindowTitle(title)
        self.setFixedSize(350, 150)
        self.setStyleSheet(f"""
            QDialog {{ background-color: #2b2b2b; color: white; }}
            QLabel {{ color: white; font-family: Arial; font-size: 11pt; }}
            QLineEdit {{ background-color: #1e1e1e; color: white; border: 1px solid #555; border-radius: 4px; padding: 6px; font-size: 11pt; }}
            QPushButton {{ background-color: {accent_color}; color: white; border-radius: 4px; padding: 6px 12px; font-size: 10pt; font-weight: bold; }}
            QPushButton:hover {{ border: 1px solid white; }}
            QPushButton#cancel {{ background-color: #555; }}
            QPushButton#cancel:hover {{ background-color: #666; border: 1px solid white; }}
        """)
        
        # Center on ACTIVE screen (fixes dual monitor popups in top right)
        from PyQt5.QtGui import QCursor
        screen = QApplication.desktop().screenGeometry(QApplication.desktop().screenNumber(QCursor.pos()))
        self.move(screen.left() + (screen.width() - 350) // 2, screen.top() + (screen.height() - 150) // 2)
        
        layout = QVBoxLayout(self)
        layout.addWidget(QLabel(label_text))
        
        self.input_field = QLineEdit(default_text)
        self.input_field.returnPressed.connect(self.accept) # Allow Enter key to submit
        layout.addWidget(self.input_field)
        
        btn_layout = QHBoxLayout()
        ok_btn = QPushButton("Apply")
        cancel_btn = QPushButton("Cancel")
        cancel_btn.setObjectName("cancel")
        
        ok_btn.clicked.connect(self.accept)
        cancel_btn.clicked.connect(self.reject)
        
        btn_layout.addStretch()
        btn_layout.addWidget(cancel_btn)
        btn_layout.addWidget(ok_btn)
        layout.addLayout(btn_layout)
        
    def get_text(self):
        return self.input_field.text()


class CustomColorDialog(QDialog):
    def __init__(self, current_color, accent_color, parent=None):
        super().__init__(None)
        self.setWindowTitle("Color & Opacity")
        self.setFixedSize(300, 260)
        self.setStyleSheet(f"""
            QDialog {{ background-color: #2b2b2b; color: white; }}
            QLabel {{ color: white; font-family: Arial; font-size: 10pt; margin-top: 10px; }}
            QPushButton {{ background-color: {accent_color}; color: white; border-radius: 4px; padding: 6px 12px; font-weight: bold; }}
            QPushButton:hover {{ border: 1px solid white; }}
            QSlider::groove:horizontal {{ border: 1px solid #555; height: 8px; background: #1e1e1e; border-radius: 4px; }}
            QSlider::handle:horizontal {{ background: {accent_color}; width: 16px; margin: -4px 0; border-radius: 8px; }}
        """)
        
        # Center on ACTIVE screen
        from PyQt5.QtGui import QCursor
        screen = QApplication.desktop().screenGeometry(QApplication.desktop().screenNumber(QCursor.pos()))
        self.move(screen.left() + (screen.width() - 300) // 2, screen.top() + (screen.height() - 220) // 2)
        
        self.layout = QVBoxLayout(self)
        
        self.color_grid = QGridLayout()
        # 24 Modern Dark Mode Colors from user palette
        self.colors = [
            "#121212", "#1F1F1F", "#2C2C2C", "#363636", "#585858", "#8D99AE",
            "#1D3557", "#457B9D", "#284B63", "#264653", "#1A535C", "#008080",
            "#2A9D8F", "#457B5D", "#228B22", "#006400", "#5E503F", "#7E705F",
            "#9A8C7A", "#A52A2A", "#8B0000", "#B22222", "#FF6347", "#FFB74D"
        ]
        
        self.selected_hex = current_color.name()[:7].upper()
        if self.selected_hex not in self.colors:
            self.selected_hex = "#1D3557"
            
        self.opacity = current_color.alpha()
        self.buttons = []
        
        row, col = 0, 0
        for c in self.colors:
            btn = QPushButton()
            btn.setFixedSize(30, 30)
            btn.clicked.connect(lambda checked, hex_val=c: self.select_color(hex_val))
            self.color_grid.addWidget(btn, row, col)
            self.buttons.append((btn, c))
            col += 1
            if col > 5:
                col = 0
                row += 1
                
        self.update_buttons()
        self.layout.addLayout(self.color_grid)
        
        self.layout.addWidget(QLabel("Opacity:"))
        self.opacity_slider = QSlider(Qt.Horizontal)
        self.opacity_slider.setRange(10, 255)
        self.opacity_slider.setValue(self.opacity)
        self.layout.addWidget(self.opacity_slider)
        
        self.ok_btn = QPushButton("Apply Configuration")
        self.ok_btn.setDefault(True) # Allow Enter key to submit
        self.ok_btn.clicked.connect(self.accept)
        self.layout.addSpacing(10)
        self.layout.addWidget(self.ok_btn)
        
    def select_color(self, hex_val):
        self.selected_hex = hex_val
        self.update_buttons()
        
    def update_buttons(self):
        for btn, c in self.buttons:
            border = "3px solid white" if c == self.selected_hex else "1px solid #555"
            btn.setStyleSheet(f"background-color: {c}; border-radius: 15px; border: {border};")
            
    def get_color(self):
        c = QColor(self.selected_hex)
        c.setAlpha(self.opacity_slider.value())
        return c

class GutterWidget(QWidget):
    clicked = pyqtSignal(str)
    config_changed = pyqtSignal()
    close_requested = pyqtSignal(str)
    add_requested = pyqtSignal(str, str)
    hover_changed = pyqtSignal()
    split_requested = pyqtSignal(str)
    reorder_requested = pyqtSignal(str, str)
    
    def __init__(self, gutter_config, engine):
        super().__init__()
        self.config = gutter_config
        self.engine = engine
        self.window_id = None
        self.hovered = False
        self.hover_y = -1
        self.drag_start_pos = None
        
        self.setWindowFlags(Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.setAttribute(Qt.WA_Hover)
        self.setMouseTracking(True)
        self.setAcceptDrops(True)
        
        self.color = QColor(self.config.get("color", "#80FFFFFF"))
        
    def enterEvent(self, event):
        self.hovered = True
        self.update()
        self.hover_changed.emit()
        
    def leaveEvent(self, event):
        self.hovered = False
        self.hover_y = -1
        self.update()
        self.hover_changed.emit()
        
    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            # Check if clicked on the hamburger icon (center screen)
            center_y = self.height() / 2 - 20
            if event.pos().y() > center_y - 15 and event.pos().y() < center_y + 15:
                self.drag_start_pos = event.pos()
        
    def mouseMoveEvent(self, event):
        self.hover_y = event.pos().y()
        self.update()
        
        if not (event.buttons() & Qt.LeftButton):
            return
            
        if hasattr(self, 'drag_start_pos') and self.drag_start_pos is not None:
            from PyQt5.QtWidgets import QApplication
            from PyQt5.QtGui import QDrag
            from PyQt5.QtCore import QMimeData
            if (event.pos() - self.drag_start_pos).manhattanLength() > QApplication.startDragDistance():
                drag = QDrag(self)
                mime = QMimeData()
                mime.setText(self.config['id'])
                drag.setMimeData(mime)
                drag.exec_(Qt.MoveAction)
                self.drag_start_pos = None
                
    def dragEnterEvent(self, event):
        if event.mimeData().hasText():
            event.acceptProposedAction()
            
    def dropEvent(self, event):
        source_id = event.mimeData().text()
        target_id = self.config['id']
        self.reorder_requested.emit(source_id, target_id)
        event.acceptProposedAction()
        
    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        # Draw the semi-transparent background
        brush = QBrush(self.color)
        painter.fillRect(self.rect(), brush)
        
        if self.hovered:
            painter.fillRect(self.rect(), QBrush(QColor(255, 255, 255, 40)))
        
        # Draw a very subtle solid border around the gutter
        painter.setPen(QColor(0, 0, 0, 40))
        painter.drawRect(0, 0, self.width() - 1, self.height() - 1)
        
        # Only draw the UI elements if the gutter is fully expanded (not auto-hidden)
        if self.width() > 15:
            # Draw 'X' close button near the bottom (shifted up slightly)
            icon_font = QFont("Arial", 9, QFont.Bold)
            painter.setFont(icon_font)
            if self.hover_y >= self.height() - 50 and self.hover_y <= self.height() - 25:
                painter.setPen(QColor(255, 100, 100)) # Red hover glow
            else:
                painter.setPen(Qt.white)
            painter.drawText(QRectF(0, self.height() - 45, self.width(), 20), Qt.AlignCenter, "X")
            
            # Draw Hamburger and Text vertically, centered on the screen
            painter.setPen(Qt.white)
            font = QFont("Arial", 10, QFont.Bold)
            painter.setFont(font)
            
            painter.save()
            # Translate to the center of the screen, shifted up slightly for the hamburger
            center_y = self.height() / 2 - 20
            painter.translate(self.width() / 2, center_y)  
            painter.rotate(90)
            
            # Draw the Hamburger icon. We check hover logic around the center Y.
            if self.hover_y > center_y - 15 and self.hover_y < center_y + 15:
                painter.setPen(QColor(100, 200, 255)) # Blue hover glow
            else:
                painter.setPen(Qt.white)
            
            font_metrics = painter.fontMetrics()
            # Draw Hamburger
            painter.drawText(0, font_metrics.descent() + 2, "☰")
            
            # Draw the name immediately after the hamburger, cascading down
            painter.setPen(Qt.white)
            text = self.config.get("name", "Gutter")
            hamburger_width = font_metrics.width("☰  ")
            painter.drawText(hamburger_width, font_metrics.descent() + 2, text)
            painter.restore()

    def mouseReleaseEvent(self, event):
        self.drag_start_pos = None
        if event.button() == Qt.LeftButton:
            if event.pos().y() <= 40:
                return
                
            # If clicked in the close button zone (shifted up)
            if event.pos().y() >= self.height() - 50 and event.pos().y() <= self.height() - 25:
                self.close_requested.emit(self.config['id'])
                return
            
            from PyQt5.QtWidgets import QApplication
            if QApplication.keyboardModifiers() & Qt.ShiftModifier:
                self.split_requested.emit(self.config['id'])
            else:
                self.clicked.emit(self.config['id'])
                
        elif event.button() == Qt.RightButton:
            self.show_context_menu(event.globalPos())

    def show_context_menu(self, pos):
        accent_color = self.color.name(QColor.HexRgb)
        
        menu = QMenu(self)
        menu.setStyleSheet(f"""
            QMenu {{
                background-color: #2b2b2b;
                color: #e0e0e0;
                border: 1px solid #444;
                border-radius: 6px;
                padding: 6px;
                font-family: Arial;
                font-size: 11pt;
            }}
            QMenu::item {{
                padding: 6px 24px 6px 12px;
                border-radius: 4px;
                margin: 2px 0px;
            }}
            QMenu::item:selected {{
                background-color: {accent_color};
                color: white;
            }}
            QMenu::separator {{
                height: 1px;
                background: #555;
                margin: 4px 8px;
            }}
        """)
        
        rename_action = menu.addAction("Rename")
        color_action = menu.addAction("Change Color & Opacity")
        cmd_action = menu.addAction("Edit Launch Command")
        menu.addSeparator()
        add_left_action = menu.addAction("Add Gutter (Left)")
        add_right_action = menu.addAction("Add Gutter (Right)")
        menu.addSeparator()
        close_deck_action = menu.addAction("Delete Gutter Deck")
        menu.addSeparator()
        quit_action = menu.addAction("Quit Application")
        
        action = menu.exec_(pos)
        
        if action == rename_action:
            dialog = CustomInputDialog("Rename Deck", "Enter new name:", self.config.get("name", ""), accent_color, self)
            if dialog.exec_() == QDialog.Accepted:
                new_name = dialog.get_text()
                if new_name:
                    self.config['name'] = new_name
                    self.update()
                    self.config_changed.emit()
                    
        elif action == color_action:
            dialog = CustomColorDialog(self.color, accent_color, self)
            if dialog.exec_() == QDialog.Accepted:
                color = dialog.get_color()
                self.color = color
                self.config['color'] = color.name(QColor.HexArgb)
                self.update()
                self.config_changed.emit()
                
        elif action == cmd_action:
            dialog = CustomInputDialog("Edit Command", "Shell command:", self.config.get("command", ""), accent_color, self)
            if dialog.exec_() == QDialog.Accepted:
                new_cmd = dialog.get_text()
                if new_cmd:
                    self.config['command'] = new_cmd
                    self.config_changed.emit()
                    
        elif action == add_left_action:
            self.add_requested.emit(self.config['id'], "left")
            
        elif action == add_right_action:
            self.add_requested.emit(self.config['id'], "right")
                
        elif action == close_deck_action:
            self.close_requested.emit(self.config['id'])
                
        elif action == quit_action:
            QApplication.instance().quit()
