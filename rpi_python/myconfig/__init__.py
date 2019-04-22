from configparser import ConfigParser
import os

CONFIG_FILE = './config.ini'

class ConfigUtil():
    def __init__(self):
        self.config = ConfigParser()
        #self.config.read(os.path.dirname(os.path.abspath(__file__)) + '/config.ini', encoding='utf-8')
        self.config.read(CONFIG_FILE)

_util = ConfigUtil()

def get(section, key):
    return _util.config.get(section, key)

def has_option(section, key):
    return _util.config.has_option(section, key)
