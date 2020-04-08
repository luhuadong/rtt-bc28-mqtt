from building import *
Import('rtconfig')

src   = []
cwd   = GetCurrentDir()

# add bc28-mqtt src files.
if GetDepend('PKG_USING_BC28_MQTT'):
    src += Glob('src/bc28_mqtt.c')

if GetDepend('PKG_USING_BC28_MQTT_SAMPLE'):
    src += Glob('examples/bc28_mqtt_sample.c')

# add bc28-mqtt include path.
path  = [cwd + '/inc']

# add src and include to group.
group = DefineGroup('bc28-mqtt', src, depend = ['PKG_USING_BC28_MQTT'], CPPPATH = path)

Return('group')
