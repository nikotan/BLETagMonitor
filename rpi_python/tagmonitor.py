#!/usr/bin/python3
# -*- coding: utf-8 -*-
import myconfig
import urllib.request, json, os, sys
import paho.mqtt.client as mqtt
import logging
from logging.handlers import TimedRotatingFileHandler
from logging import StreamHandler
from datetime import datetime


# setup logger
logger = logging.getLogger()
formatter = logging.Formatter(
    '[%(levelname)s] %(asctime)s %(filename)s %(funcName)s(%(lineno)d) : %(message)s'
)
handler = TimedRotatingFileHandler(
    filename="log/log",
    when="D",
    interval=1,
    backupCount=7,
)
#handler = StreamHandler()
handler.setFormatter(formatter)
logger.addHandler(handler)
logger.setLevel(logging.INFO)

# "tags"
tags = {}


# on_connect triggered by MQTT client
def on_connect(client, userdata, flags, respons_code):
    logger.info('MQTT status: {0}'.format(respons_code))
    mqtt_topic = myconfig.get('mqtt', 'topic')
    client.subscribe(mqtt_topic)
    logger.info('MQTT subscribed: {}'.format(mqtt_topic))


# on_message triggered by MQTT client
def on_message(client, userdata, msg):
    payload = json.loads(msg.payload.decode("utf-8"))
    logger.info('MQTT recieved payload: {}'.format(payload))

    # prepare timeout check
    timeout_sec = int(myconfig.get('monitor', 'timeout_sec'))
    now = int(datetime.now().timestamp())

    # update "tags"
    global tags
    if 'monitor_name' in payload and 'tag_detected' in payload:
        monitor_name = payload['monitor_name']
        for tag in payload['tag_detected']:
            if tag in tags:
                if 'time' not in tags[tag]:
                    logger.info('send IFTTT event: {}({}) arrived!'.format(tag, tags[tag]['name']))
                    sendIftttEvent(1, tags[tag]['name'], monitor_name)
                tags[tag]['time'] = now
    for tag in tags:
        if 'time' in tags[tag]:
            logger.info('time: {}sec ({})'.format(now - tags[tag]['time'], tag))
            if tags[tag]['time'] < now - timeout_sec:
                logger.info('send IFTTT event: {}({}) leaved!'.format(tag, tags[tag]['name']))
                sendIftttEvent(0, tags[tag]['name'], '')
                tags[tag].pop('time')


# send IFTTT webhook event
def sendIftttEvent(state, tag_name, monitor_name):
    url_format = myconfig.get('ifttt', 'url_format')
    event = myconfig.get('ifttt', 'event')
    key = myconfig.get('ifttt', 'key')
    url = url_format.format(event, key)

    msg_arrive_format = myconfig.get('ifttt', 'msg_arrive_format')
    msg_leave_format  = myconfig.get('ifttt', 'msg_leave_format')

    msg = ''
    if state == 0:
        msg = msg_leave_format.format(tag_name)
    else:
        msg = msg_arrive_format.format(tag_name, monitor_name)

    obj = {"value1" : msg}
    json_data = json.dumps(obj).encode("utf-8")
    method = "POST"
    headers = {"Content-Type" : "application/json"}
    request = urllib.request.Request(url, data=json_data, method=method, headers=headers)
    with urllib.request.urlopen(request) as response:
        response_body = response.read().decode("utf-8")
        logger.info('IFTTT response: {}'.format(response_body))


if __name__ == '__main__':

    # initialize "tags"
    with open('tags.json', 'r') as f:
        tags = json.loads(f.read())
    logger.info('initialized tags: {}'.format(tags))

    # setup MQTT client
    mqtt_host = myconfig.get('mqtt', 'host')
    mqtt_port = int(myconfig.get('mqtt', 'port'))
    client = mqtt.Client(
        client_id='tagmonitor',
        clean_session=True,
        protocol=mqtt.MQTTv311
    )
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(mqtt_host, port=mqtt_port, keepalive=60)
    client.loop_forever()
