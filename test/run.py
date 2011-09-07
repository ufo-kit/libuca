#!/usr/bin/env python

import Tkinter
import threading
import subprocess

class App(object):
    def __init__(self, parent):
        self.parent = parent
        self.canvas = Tkinter.Canvas(root, width=300, height=300)
        self.canvas.pack()
        self.rect = self.canvas.create_rectangle(0, 0, 300, 300, fill='yellow')
        self.run_button = Tkinter.Button(parent, text='Start grabbing', command=self.run_click)
        self.run_button.pack()
        
    def run_grab(self):
        proc = subprocess.Popen(['./grab'])
        proc.wait()
        self.canvas.itemconfig(self.rect, fill='red')
        self.run_button.config(state=Tkinter.NORMAL, text='Close', command=self.parent.destroy)
        
    def run_click(self):
        self.canvas.itemconfig(self.rect, fill='green')
        self.run_button.config(state=Tkinter.DISABLED)
        thread = threading.Thread(None, self.run_grab)
        thread.start()

if __name__ == '__main__':
    root = Tkinter.Tk()
    app = App(root)
    root.mainloop()
