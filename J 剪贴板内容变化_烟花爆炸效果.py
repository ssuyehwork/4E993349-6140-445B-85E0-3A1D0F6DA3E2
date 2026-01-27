#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import subprocess
import sys
import os
import hashlib
import random
import math
from datetime import datetime
import time

# ==========================================
# 1. 依赖自动检测与安装
# ==========================================
def check_and_install_dependencies():
    """检测并自动安装依赖包，保证程序稳健运行"""
    required_packages = {
        'PyQt5': 'PyQt5',
        'pillow': 'pillow',
        'pyautogui': 'pyautogui'
    }
    
    missing_packages = []
    
    for package_name, pip_name in required_packages.items():
        try:
            if package_name == 'pillow':
                __import__('PIL')
            else:
                __import__(package_name)
            print(f"✓ {package_name} 已安装")
        except ImportError:
            missing_packages.append(pip_name)
            print(f"✗ {package_name} 未安装")
    
    if missing_packages:
        print(f"\n正在安装缺失的依赖包: {', '.join(missing_packages)}")
        print("请稍候，这可能需要几分钟...\n")
        try:
            subprocess.check_call([
                sys.executable, 
                "-m", 
                "pip", 
                "install", 
                *missing_packages
            ])
            print("\n✓ 所有依赖包安装完成！")
            print("正在启动程序...\n")
        except subprocess.CalledProcessError as e:
            print(f"\n✗ 安装失败: {e}")
            input("按回车键退出...")
            sys.exit(1)
    else:
        print("\n✓ 所有依赖包已就绪！\n")

check_and_install_dependencies()

from PyQt5.QtWidgets import (QApplication, QSystemTrayIcon, QMenu, QAction, 
                             QLabel, QWidget, QVBoxLayout, QHBoxLayout, 
                             QPushButton, QListWidget, QListWidgetItem, QFrame, QSizeGrip)
from PyQt5.QtCore import QTimer, Qt, QPropertyAnimation, QRect, QEasingCurve, pyqtSignal, QThread, QPoint, QSettings
from PyQt5.QtGui import QIcon, QPixmap, QPainter, QColor, QFont, QCursor, QPalette, QBrush, QPen, QPainterPath
import pyautogui

# ==========================================
# 2. 剪贴板监听线程 (核心底层)
# ==========================================
class ClipboardWatcher(QThread):
    """独立线程监控剪贴板，防止卡顿主界面"""
    clipboard_changed = pyqtSignal(object)
    
    def __init__(self):
        super().__init__()
        self.running = True
        self.last_hash = ""
        
    def run(self):
        """监控线程主循环"""
        while self.running:
            try:
                clipboard = QApplication.clipboard()
                current_hash = self.get_clipboard_hash(clipboard)
                
                if current_hash and current_hash != self.last_hash:
                    self.last_hash = current_hash
                    self.clipboard_changed.emit(clipboard.mimeData())
                    
            except Exception as e:
                print(f"监控错误: {e}")
            self.msleep(300)
    
    def get_clipboard_hash(self, clipboard):
        """计算剪贴板内容的唯一哈希值"""
        try:
            mime = clipboard.mimeData()
            if not mime: return ""
            hash_parts = []
            if mime.hasText(): hash_parts.append(mime.text())
            if mime.hasImage():
                img = mime.imageData()
                if img: hash_parts.append(f"img_{img.width()}x{img.height()}")
            if mime.hasUrls(): hash_parts.append(",".join([u.toString() for u in mime.urls()]))
            if mime.hasHtml(): hash_parts.append(mime.html()[:100])
            for fmt in mime.formats(): hash_parts.append(fmt)
            content = "|".join(hash_parts)
            return hashlib.md5(content.encode('utf-8', errors='ignore')).hexdigest()
        except: return ""
    
    def stop(self): self.running = False

# ==========================================
# 3. 视觉特效：微缩闪烁粒子引擎 (Sparkle & Flash)
# ==========================================
class Particle:
    """单个粒子物理行为定义 (微缩 + 闪烁)"""
    def __init__(self, x, y, style, index=0, total=1):
        self.x = x; self.y = y; self.initial_x = x; self.initial_y = y
        self.style = style; self.index = index; self.total = total
        self.age = 0; self.active = True
        
        # --- 默认物理参数 (微缩版) ---
        self.vx = 0; self.vy = 0; self.gravity = 0
        self.drag = 0.92 # 高阻力，快速停下
        self.size = random.uniform(1.5, 3.5) # 小尺寸
        self.decay = 4 # 快消逝
        self.color = QColor(255, 255, 255)
        self.rotation = 0; self.spin = 0; self.char = ""; self.width_factor = 1.0; self.phase = 0
        
        # --- 闪烁属性 ---
        self.shimmer = True # 默认开启闪烁
        self.shimmer_speed = random.uniform(0.3, 0.8)

        # --- 样式逻辑 ---
        if style == 'butterfly': # 🦋 幻彩迷蝶
            angle = random.uniform(0, 6.28); speed = random.uniform(1, 3)
            self.vx = math.cos(angle) * speed; self.vy = math.sin(angle) * speed
            self.gravity = 0.01; self.drag = 0.96
            self.color = QColor.fromHsv(random.randint(0, 359), 220, 255) # 高饱和度
            self.size = random.uniform(3, 5); self.decay = 2.0
            self.phase = random.uniform(0, 3.14)
            
        elif style == 'matrix': # 📟 黑客帝国
            self.char = random.choice(['0', '1', 'C', 'O', 'P', 'Y', 'X'])
            self.vy = random.uniform(3, 6); self.color = QColor(0, 255, 70)
            self.size = random.randint(8, 12); self.decay = 5
            self.shimmer = False # 代码不闪烁，保持清晰
            
        elif style == 'dna': # 🧬 生命螺旋
            self.vy = -random.uniform(1, 3); self.phase = (index / total) * 4 * math.pi
            self.amp = random.uniform(10, 15); self.decay = 3
            self.color = QColor(0, 200, 255) if index % 2 == 0 else QColor(255, 0, 150)
            
        elif style == 'lightning': # ⚡ 雷霆之怒 (爆闪)
            angle = random.uniform(0, 2 * math.pi); dist = random.uniform(20, 60)
            self.target_x = x + math.cos(angle) * dist; self.target_y = y + math.sin(angle) * dist
            self.points = []; steps = 4
            for i in range(steps):
                t = (i + 1) / steps
                nx = x + (self.target_x - x) * t + random.uniform(-10, 10)
                ny = y + (self.target_y - y) * t + random.uniform(-10, 10)
                self.points.append((nx, ny))
            self.color = QColor(220, 220, 255); self.decay = 20
            self.shimmer = True # 极度闪烁
            
        elif style == 'confetti': # 🎊 节日礼炮
            angle = random.uniform(0, 6.28); speed = random.uniform(2, 6)
            self.vx = math.cos(angle) * speed; self.vy = math.sin(angle) * speed
            self.gravity = 0.2; self.drag = 0.92; self.spin = random.uniform(-0.2, 0.2)
            self.color = QColor.fromHsv(random.randint(0, 359), 200, 255); self.size = random.uniform(4, 7)
            
        elif style == 'void': # 🌀 虚空奇点
            angle = random.uniform(0, 6.28); dist = random.uniform(40, 80)
            self.x = x + math.cos(angle) * dist; self.y = y + math.sin(angle) * dist
            self.vx = (self.initial_x - self.x) * 0.15; self.vy = (self.initial_y - self.y) * 0.15
            self.color = QColor(150, 0, 255); self.mode = 'suck'; self.decay = 0
            
        elif style == 'heart': # ❤️ 怦然心动
            t = (index / total) * 2 * math.pi; scale = random.uniform(1.0, 1.8)
            self.vx = (16 * math.sin(t)**3) * 0.1 * scale
            self.vy = -(13 * math.cos(t) - 5 * math.cos(2*t) - 2 * math.cos(3*t) - math.cos(4*t)) * 0.1 * scale
            self.gravity = 0.02; self.color = QColor(255, 80, 150); self.decay = 3
            
        elif style == 'galaxy': # 🌌 银河螺旋
            arm = index % 3; angle = (arm * 2.09) + (index / total) + random.uniform(-0.2, 0.2)
            speed = random.uniform(1, 3)
            self.vx = math.cos(angle) * speed; self.vy = math.sin(angle) * speed
            self.color = QColor.fromHsv(random.randint(200, 300), 220, 255); self.decay = 4
            
        elif style == 'frozen': # ❄️ 冰霜新星
            angle = random.uniform(0, 6.28); speed = random.uniform(5, 12)
            self.vx = math.cos(angle) * speed; self.vy = math.sin(angle) * speed
            self.gravity = 0.05; self.drag = 0.80; self.color = QColor(200, 255, 255); self.decay = 5
            
        elif style == 'phoenix': # 🔥 凤凰涅槃
            angle = random.uniform(math.pi + 0.5, 2 * math.pi - 0.5); speed = random.uniform(1, 4)
            self.vx = math.cos(angle) * speed; self.vy = math.sin(angle) * speed
            self.gravity = -0.1; self.color = QColor(255, random.randint(150, 255), 50); self.decay = 4
            
        elif style == 'chaos': # 🌪️ 混沌
            self.vx = random.uniform(-2, 2); self.vy = random.uniform(-2, 2)
            self.drag = 0.98; self.color = QColor(255, 50, 50); self.decay = 6
            
        else: # neon/gold/default
            angle = random.uniform(0, 6.28); speed = random.uniform(1, 5)
            self.vx = math.cos(angle) * speed; self.vy = math.sin(angle) * speed
            self.gravity = 0.15
            if style == 'gold':
                self.color = QColor(255, 235, 100); self.gravity = 0.25
            else:
                self.color = QColor.fromHsv(random.randint(0, 359), 220, 255)
        
        self.alpha = 255

    def update(self):
        """更新粒子状态"""
        if self.style == 'butterfly':
            self.x += self.vx; self.y += self.vy; self.vy += self.gravity
            self.vx *= self.drag; self.vy *= self.drag
            self.x += math.sin(self.age * 0.2 + self.phase) * 0.8
            self.alpha -= self.decay; self.age += 1
        elif self.style == 'dna':
            self.y += self.vy; self.age += 1; offset = math.sin((self.y * 0.05) + self.phase) * self.amp
            self.x = self.initial_x + offset; self.alpha = 255 if offset > 0 else 100
            if self.age > 60: self.alpha = 0
        elif self.style == 'lightning': self.alpha -= self.decay
        elif self.style == 'confetti':
            self.x += self.vx; self.y += self.vy; self.vy += self.gravity
            self.vx *= self.drag; self.vy *= self.drag
            self.rotation += self.spin; self.width_factor = abs(math.cos(self.rotation)); self.alpha -= 2
        elif self.style == 'void':
            if self.mode == 'suck':
                self.x += self.vx; self.y += self.vy
                if math.sqrt((self.x - self.initial_x)**2 + (self.y - self.initial_y)**2) < 5:
                    self.mode = 'boom'; angle = random.uniform(0, 6.28); speed = random.uniform(2, 8)
                    self.vx = math.cos(angle) * speed; self.vy = math.sin(angle) * speed; self.color = QColor(255, 255, 255)
            else: self.x += self.vx; self.y += self.vy; self.alpha -= 5
        elif self.style == 'phoenix':
            self.x += self.vx; self.y += self.vy; self.vy += self.gravity; self.vx *= self.drag; self.vy *= self.drag
            if self.age > 10 and self.color.green() > 50: self.color.setGreen(self.color.green() - 5)
            self.alpha -= self.decay; self.age += 1
        else:
            self.x += self.vx; self.y += self.vy; self.vy += self.gravity
            self.vx *= self.drag; self.vy *= self.drag; self.alpha -= self.decay
        return self.alpha > 0

class FireworksOverlay(QWidget):
    """全屏特效层 (无圆环，有闪烁)"""
    def __init__(self):
        super().__init__()
        self.setWindowFlags(Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool | Qt.WindowTransparentForInput)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.particles = []; self.timer = QTimer(); self.timer.timeout.connect(self.animate)
        self.resize(QApplication.desktop().size())
        
    def explode(self, pos):
        self.setGeometry(QApplication.desktop().geometry()); self.show()
        lp = self.mapFromGlobal(pos)
        # 移除了所有圆环类特效
        styles = ['neon', 'gold', 'butterfly', 'quantum', 'heart', 'galaxy', 'frozen', 'phoenix', 
                  'matrix', 'dna', 'lightning', 'void', 'confetti', 'chaos']
        style = random.choice(styles)
        
        count = 40
        if style in ['matrix']: count = 15
        elif style in ['dna', 'lightning', 'butterfly']: count = 30
        elif style in ['heart', 'galaxy']: count = 60
        
        for i in range(count): self.particles.append(Particle(lp.x(), lp.y(), style, i, count))
        if not self.timer.isActive(): self.timer.start(16)

    def animate(self):
        if not self.particles: self.timer.stop(); self.hide(); return
        self.particles = [p for p in self.particles if p.update()]; self.update()
        
    def paintEvent(self, event):
        if not self.particles: return
        p = QPainter(self); p.setRenderHint(QPainter.Antialiasing); p.setCompositionMode(QPainter.CompositionMode_Plus)
        for pt in self.particles:
            # 闪烁逻辑：随机抖动透明度，制造 Blink 效果
            alpha_val = int(pt.alpha)
            if pt.shimmer:
                flicker = random.uniform(0.6, 1.0) # 亮度抖动
                alpha_val = int(pt.alpha * flicker)
            
            c = QColor(pt.color); c.setAlpha(max(0, alpha_val)); p.setPen(Qt.NoPen); p.setBrush(c)
            
            if pt.style == 'butterfly':
                flap = abs(math.sin(pt.age * 0.3 + pt.phase))
                p.save(); p.translate(pt.x, pt.y)
                angle = math.atan2(pt.vy, pt.vx); p.rotate(math.degrees(angle) + 90)
                w = pt.size * flap; h = pt.size
                p.setBrush(c); p.drawEllipse(QPoint(int(-w), 0), int(w), int(h)); p.drawEllipse(QPoint(int(w), 0), int(w), int(h))
                p.restore()
            elif pt.style == 'matrix':
                p.setPen(c); f = QFont("Consolas", int(pt.size)); f.setBold(True); p.setFont(f); p.drawText(int(pt.x), int(pt.y), pt.char)
            elif pt.style == 'lightning':
                p.setPen(QPen(c, 1.5)); path = QPainterPath(); path.moveTo(pt.initial_x, pt.initial_y)
                for px, py in pt.points: path.lineTo(px, py)
                p.drawPath(path)
            elif pt.style == 'confetti':
                p.save(); p.translate(pt.x, pt.y); p.rotate(math.degrees(pt.rotation))
                w = 6 * pt.width_factor; h = 10
                p.drawRect(int(-w / 2), int(-h / 2), int(w), int(h)); p.restore()
            elif pt.style == 'quantum':
                s = pt.size * (pt.alpha / 255.0); p.drawRect(int(pt.x - s / 2), int(pt.y - s / 2), int(s), int(s))
            elif pt.style == 'gold':
                p.setPen(QPen(c, pt.size)); p.drawLine(int(pt.x), int(pt.y), int(pt.x - pt.vx), int(pt.y - pt.vy))
            else:
                p.drawEllipse(int(pt.x), int(pt.y), int(pt.size), int(pt.size))

# ==========================================
# 4. 悬浮面板 (全功能还原 + 闪烁感应)
# ==========================================
class FloatingPanel(QWidget):
    request_paste = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self.setWindowFlags(Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool)
        self.setAttribute(Qt.WA_TranslucentBackground)
        
        self.dragging = False; self.drag_position = QPoint()
        self.settings = QSettings("ClipboardMonitor", "FloatingPanel")
        
        # 主布局
        main_layout = QVBoxLayout(); main_layout.setContentsMargins(0, 0, 0, 0); main_layout.setSpacing(0)
        
        # 容器 (用于闪烁特效)
        self.container = QFrame()
        # 默认样式
        self.default_style = """
            QFrame {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1E1E1E, stop:1 #121212);
                border-radius: 12px;
                border: 1px solid #333333;
            }
        """
        # 高亮样式 (闪烁时使用)
        self.flash_style = """
            QFrame {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #252525, stop:1 #1A1A1A);
                border-radius: 12px;
                border: 2px solid #00E5FF; /* 亮青色边框 */
            }
        """
        self.container.setStyleSheet(self.default_style)
        
        container_layout = QVBoxLayout(); container_layout.setContentsMargins(0, 0, 0, 0); container_layout.setSpacing(0)
        
        # 标题栏
        title_bar = QFrame(); title_bar.setFixedHeight(40)
        title_bar.setStyleSheet("background: rgba(66, 66, 66, 0.5); border-top-left-radius: 12px; border-top-right-radius: 12px;")
        
        title_layout = QHBoxLayout(); title_layout.setContentsMargins(15, 0, 10, 0)
        title_label = QLabel("📋 剪贴板历史"); title_label.setFont(QFont("Microsoft YaHei", 11, QFont.Bold)); title_label.setStyleSheet("color: #E0E0E0;")
        self.count_label = QLabel("0 项"); self.count_label.setFont(QFont("Microsoft YaHei", 9)); self.count_label.setStyleSheet("color: #9E9E9E;")
        
        btn_style = "QPushButton { background: rgba(255, 255, 255, 0.1); color: #E0E0E0; border: none; border-radius: 15px; font-weight: bold; font-size: 16pt; } QPushButton:hover { background: rgba(255, 255, 255, 0.2); }"
        minimize_btn = QPushButton("—"); minimize_btn.setFixedSize(30, 30); minimize_btn.setStyleSheet(btn_style); minimize_btn.clicked.connect(self.hide)
        close_btn = QPushButton("×"); close_btn.setFixedSize(30, 30); close_btn.setStyleSheet(btn_style); close_btn.clicked.connect(self.hide)
        
        title_layout.addWidget(title_label); title_layout.addWidget(self.count_label); title_layout.addStretch()
        title_layout.addWidget(minimize_btn); title_layout.addWidget(close_btn)
        title_bar.setLayout(title_layout)
        
        # 列表
        self.history_list = QListWidget()
        self.history_list.setStyleSheet("QListWidget { background: #1E1E1E; border: none; padding: 8px; font-size: 10pt; outline: none; color: #E0E0E0; } QListWidget::item { background: #2A2A2A; padding: 12px; border-radius: 6px; margin: 4px 2px; } QListWidget::item:hover { background: #333333; border-left: 3px solid #64B5F6; } QListWidget::item:selected { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1976D2, stop:1 #2196F3); color: white; border-left: 3px solid #64B5F6; }")
        self.history_list.setAlternatingRowColors(False)
        self.history_list.itemDoubleClicked.connect(self.paste_to_active_window)
        
        # 底部
        bottom_bar = QFrame(); bottom_bar.setFixedHeight(45)
        bottom_bar.setStyleSheet("background: rgba(66, 66, 66, 0.5); border-bottom-left-radius: 12px; border-bottom-right-radius: 12px;")
        bottom_layout = QHBoxLayout(); bottom_layout.setContentsMargins(10, 5, 10, 5)
        clear_btn = QPushButton("🗑 清空"); clear_btn.setFixedHeight(35)
        clear_btn.setStyleSheet("QPushButton { background: rgba(244, 67, 54, 0.3); color: #E0E0E0; border: 1px solid rgba(244, 67, 54, 0.5); border-radius: 5px; padding: 5px 15px; font-weight: bold; } QPushButton:hover { background: rgba(244, 67, 54, 0.5); }")
        clear_btn.clicked.connect(self.clear_history)
        bottom_layout.addWidget(QLabel("💡 双击项目快速粘贴")); bottom_layout.addStretch(); bottom_layout.addWidget(clear_btn)
        bottom_bar.setLayout(bottom_layout)
        
        container_layout.addWidget(title_bar); container_layout.addWidget(self.history_list); container_layout.addWidget(bottom_bar)
        self.container.setLayout(container_layout); main_layout.addWidget(self.container)
        self.setLayout(main_layout)
        
        self.size_grip = QSizeGrip(self); self.size_grip.setStyleSheet("background: transparent; width: 16px; height: 16px;")
        self.history_data = []; self.load_window_state()
    
    # --- 闪烁逻辑 ---
    def trigger_flash(self):
        """触发面板边框闪烁"""
        self.container.setStyleSheet(self.flash_style)
        QTimer.singleShot(300, lambda: self.container.setStyleSheet(self.default_style))

    # --- 窗口逻辑 ---
    def resizeEvent(self, event): super().resizeEvent(event); self.size_grip.move(self.width()-16, self.height()-16)
    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton: self.dragging = True; self.drag_position = event.globalPos() - self.frameGeometry().topLeft(); event.accept()
    def mouseMoveEvent(self, event):
        if self.dragging: self.move(event.globalPos() - self.drag_position); event.accept()
    def mouseReleaseEvent(self, event): self.dragging = False
    
    def load_window_state(self):
        width = self.settings.value("width", 400, type=int); height = self.settings.value("height", 600, type=int)
        self.resize(width, height)
        x = self.settings.value("x", None); y = self.settings.value("y", None)
        if x and y: self.move(int(x), int(y))
        else: self.move_to_right_edge()
    
    def save_window_state(self):
        self.settings.setValue("width", self.width()); self.settings.setValue("height", self.height())
        self.settings.setValue("x", self.x()); self.settings.setValue("y", self.y())
    
    def move_to_right_edge(self):
        screen = QApplication.desktop().screenGeometry()
        self.move(screen.width() - self.width() - 20, (screen.height() - self.height()) // 2)

    def add_history(self, icon, content_type, preview, full_content):
        timestamp = datetime.now().strftime("%H:%M:%S")
        item = QListWidgetItem(); item.setText(f"{icon} [{timestamp}] {content_type}\n{preview[:80]}")
        item.setData(Qt.UserRole, full_content)
        self.history_list.insertItem(0, item)
        self.history_data.insert(0, {'content': full_content, 'type': content_type, 'time': timestamp})
        if self.history_list.count() > 50: self.history_list.takeItem(50); self.history_data.pop(50)
        self.count_label.setText(f"{self.history_list.count()} 项")
        # 触发闪烁
        self.trigger_flash()
    
    def paste_to_active_window(self, item):
        content = item.data(Qt.UserRole)
        if content: self.request_paste.emit(content)
            
    def clear_history(self):
        self.history_list.clear(); self.history_data.clear(); self.count_label.setText("0 项")
    
    def closeEvent(self, e): self.save_window_state(); e.ignore(); self.hide()
    def hideEvent(self, e): self.save_window_state(); super().hideEvent(e)
    def moveEvent(self, e):
        super().moveEvent(e)
        if hasattr(self, '_save_timer'): self._save_timer.stop()
        else: self._save_timer = QTimer(); self._save_timer.setSingleShot(True); self._save_timer.timeout.connect(self.save_window_state)
        self._save_timer.start(500)

# ==========================================
# 5. 主控制器 (详细分析功能完整)
# ==========================================
class ClipboardMonitor:
    def __init__(self):
        self.app = QApplication(sys.argv)
        self.app.setQuitOnLastWindowClosed(False)
        self.copy_count = 0
        self.ignore_next = False
        
        self.fireworks = FireworksOverlay()
        self.floating_panel = FloatingPanel()
        self.floating_panel.request_paste.connect(self.handle_paste_request)
        
        self.tray_icon = QSystemTrayIcon(self.create_icon(), self.app)
        self.create_menu()
        self.tray_icon.show()
        
        self.watcher = ClipboardWatcher()
        self.watcher.clipboard_changed.connect(self.on_clipboard_changed)
        self.watcher.start()
        
        print("✓ 监控系统就绪 | 闪烁特效已装载 | 圆环已移除")
        print("✓ 左膀右臂(双击粘贴)完美修复 💪\n")
    
    def create_icon(self):
        pixmap = QPixmap(64, 64); pixmap.fill(Qt.transparent)
        painter = QPainter(pixmap); painter.setRenderHint(QPainter.Antialiasing)
        painter.setBrush(QColor(30, 30, 30)); painter.setPen(Qt.NoPen); painter.drawEllipse(4, 4, 56, 56)
        painter.setPen(QColor(255, 255, 255)); painter.setFont(QFont("Segoe UI Emoji", 32))
        painter.drawText(QRect(0, 0, 64, 64), Qt.AlignCenter, "💻"); painter.end(); return QIcon(pixmap)
    
    def create_menu(self):
        menu = QMenu()
        show_panel = QAction("📋 显示面板", self.app); show_panel.triggered.connect(self.show_panel); menu.addAction(show_panel)
        menu.addSeparator()
        self.count_action = QAction(f"📊 已复制: {self.copy_count} 次", self.app); self.count_action.setEnabled(False); menu.addAction(self.count_action)
        menu.addSeparator()
        clear_action = QAction("🗑 清空剪贴板", self.app); clear_action.triggered.connect(self.clear_clipboard); menu.addAction(clear_action)
        menu.addSeparator()
        quit_action = QAction("❌ 退出程序", self.app); quit_action.triggered.connect(self.quit_app); menu.addAction(quit_action)
        self.tray_icon.setContextMenu(menu); self.tray_icon.activated.connect(self.on_tray_activated)
    
    def on_tray_activated(self, reason):
        if reason in (QSystemTrayIcon.Trigger, QSystemTrayIcon.DoubleClick): self.show_panel()
    
    def show_panel(self): self.floating_panel.show(); self.floating_panel.activateWindow()
        
    def handle_paste_request(self, content):
        """双击粘贴逻辑：闪电最小化战术，确保不关闭面板但移交焦点"""
        self.ignore_next = True
        try:
            QApplication.clipboard().setText(content)
            self.floating_panel.showMinimized() # 最小化让出焦点
            QApplication.processEvents()
            time.sleep(0.05)
            pyautogui.hotkey('ctrl', 'v')
            time.sleep(0.05)
            self.floating_panel.showNormal() # 瞬间还原
            self.floating_panel.activateWindow()
            print(f"✓ 已粘贴")
        except Exception as e:
            print(f"粘贴出错: {e}")
            self.floating_panel.showNormal()
            self.ignore_next = False
    
    def on_clipboard_changed(self, mime_data):
        if self.ignore_next: self.ignore_next = False; return
        try:
            self.copy_count += 1
            self.count_action.setText(f"📊 已复制: {self.copy_count} 次")
            
            self.fireworks.explode(QCursor.pos()) # 播放闪烁特效
            content_type, icon, preview, full_content = self.analyze_clipboard(mime_data)
            self.floating_panel.add_history(icon, content_type, preview, full_content)
            
            print(f"[{datetime.now().strftime('%H:%M:%S')}] {icon} {content_type}: {preview[:60]}")
        except Exception as e: print(f"错误: {e}")
    
    def analyze_clipboard(self, mime_data):
        """详细的剪贴板分析逻辑"""
        if mime_data.hasImage():
            img = mime_data.imageData(); w, h = img.width(), img.height()
            return ("图片", "🖼️", f"{w} × {h} 像素", f"img_{w}x{h}")
        if mime_data.hasUrls():
            urls = mime_data.urls()
            if len(urls) == 1:
                path = urls[0].toLocalFile()
                if path:
                    name = os.path.basename(path); ext = os.path.splitext(name)[1].lower()
                    file_types = {
                        'image': (['.jpg', '.png', '.gif', '.bmp'], "🖼️"), 'video': (['.mp4', '.avi', '.mkv', '.mov'], "🎬"),
                        'audio': (['.mp3', '.wav', '.flac'], "🎵"), 'doc': (['.pdf', '.doc', '.docx', '.txt'], "📝"),
                        'archive': (['.zip', '.rar', '.7z'], "📦")
                    }
                    found_icon = "📄"
                    for ftype, (exts, icon) in file_types.items():
                        if ext in exts: found_icon = icon; break
                    return (f"{os.path.splitext(name)[1][1:].upper()}文件", found_icon, name, path)
                else: return ("链接", "🔗", urls[0].toString()[:60], urls[0].toString())
            else: return ("多个文件", "📁", f"共 {len(urls)} 项", "\n".join([u.toLocalFile() or u.toString() for u in urls]))
        if mime_data.hasText():
            text = mime_data.text(); length = len(text); lines = text.count('\n') + 1
            if text.startswith(('http://', 'https://')): return ("网址", "🔗", text[:60], text)
            if '@' in text and '.' in text and len(text.split()) == 1: return ("邮箱", "📧", text, text)
            if text.replace('.', '').replace('-', '').isdigit(): return ("数字", "🔢", text, text)
            if any(kw in text for kw in ['def ', 'class ', 'import ', 'function', 'const ', 'var ', 'let ', '<?php']): return ("代码", "💻", text[:60], text)
            if any(kw in text.lower() for kw in ['select ', 'insert ', 'update ', 'delete ', 'create table']): return ("SQL", "💾", text[:60], text)
            if lines > 3: return (f"多行文本({lines}行)", "📄", text[:60], text)
            return ("文本", "📝", text[:60], text)
        if mime_data.hasHtml(): return ("HTML", "🌐", mime_data.html()[:60], mime_data.html())
        return ("未知类型", "❓", "", "")
    
    def clear_clipboard(self): QApplication.clipboard().clear(); self.fireworks.explode(QCursor.pos())
    def quit_app(self): self.watcher.stop(); self.watcher.wait(); self.tray_icon.hide(); self.app.quit()
    def run(self):
        self.floating_panel.show()
        screen = QApplication.desktop().screenGeometry(); self.fireworks.explode(screen.center())
        sys.exit(self.app.exec_())

if __name__ == "__main__":
    monitor = ClipboardMonitor()
    monitor.run()