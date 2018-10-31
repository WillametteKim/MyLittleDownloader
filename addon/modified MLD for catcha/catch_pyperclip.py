#OK@windows 10(1703) + python 3.6
#THX TO https://stackoverflow.com/questions/14685999/trigger-an-event-when-clipboard-content-changes
#THX TO https://stackoverflow.com/questions/434597/open-document-with-default-application-in-python

import ctypes, time
import subprocess, os, sys
sys.path.append(os.path.abspath("SO_site-packages"))
import pyperclip

#check wheter clipboard text is really "link" or not.
def check_url(link):
	return link.find('http')

def open_extern_program(filepath, link):
    if sys.platform.startswith('darwin'):
        subprocess.call(('open', filepath))
    elif os.name == 'nt':
        os.startfile(filepath)
    elif os.name == 'posix':
        subprocess.call(('xdg-open', "usr/bin/gedit"))

filepath = os.path.dirname( os.path.abspath( __file__ ) )
print(filepath)
filepath = filepath + "/Ongoing.exe" 
print(filepath)

recent_value = ""
while True: 
    tmp_value = pyperclip.paste()
    if not(check_url(tmp_value)): #if link is vallid URL
        if tmp_value != recent_value: #if URL is new LINK
            recent_value = tmp_value
            print(recent_value)
            open_extern_program(filepath, recent_value)
    time.sleep(1)