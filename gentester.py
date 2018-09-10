#!/usr/bin/python

#from collections import OrderedDict

hostnamelst = ['net06.utdallas.edu', 'net07.utdallas.edu', 'net08.utdallas.edu']

tcpportbase = 6000
udpportbase = 7000

iotnodeport = 8000
iotnodeinterval = 200
iotnodeforwlim = 10
sleepsecs = 300

nodecount = 5
edges = ((1, 2), (2, 3), (3, 4), (3, 5), (4, 5))


fognodes = {}
fognodeslst = []

for i in xrange(1, nodecount+1):
    for hostname in hostnamelst:
        peerslst = []
        for edge in edges:
            if edge[0] == i or edge[1] == i:
                if edge[0] == i:
                    peerno = edge[1]
                else:
                    peerno = edge[0]
                peerslst.append({ 'hostname' : hostname, 'port' : tcpportbase + peerno })
        for peerhostname in hostnamelst:
            if hostname != peerhostname:
                peerslst.append({ 'hostname' : peerhostname, 'port' : tcpportbase + i })
        fognode = { 'maxresponsetime' : 10 * i, 'updateinterval' : i, 'hostname' : hostname,
                        'tcpport' : tcpportbase + i, 'udpport' : udpportbase + i, 'peers' : peerslst }
        fognodes.setdefault(hostname, []).append(fognode)
        fognodeslst.append(fognode)

shellscript = '''#!/bin/sh

#
# Generated using gentester.py
#

if [ -z $HOSTNAME ]; then
    HOSTNAME=$(hostname)
fi

if [ -z $UID ]; then
    UID=$(id -u)
fi

echo Creating logs folder...

mkdir -p logs

echo Generating testconfig.txt ...

cat > testconfig.txt << EOL
$HOSTNAME
{port}
{interval}
{forwlim}
{numfognodes}
{strfognodes}
EOL

echo Running fog nodes for $HOSTNAME...

'''.format(port = iotnodeport, interval = iotnodeinterval, forwlim = iotnodeforwlim, numfognodes = len(fognodeslst), 
        strfognodes = '\n'.join([fn['hostname'] + '\n' + str(fn['udpport']) for fn in fognodeslst]))

firstHost = True
for (hostname, fognodes) in  fognodes.iteritems():
    shellscript += ('el' if not firstHost else '') + 'if [ "$HOSTNAME" = "{}" ]; then\n'.format(hostname)
    for fognode in fognodes:
        shellscript += '\t./FogNode {} {} {} {} {} \\\n'.format(
                fognode['maxresponsetime'], fognode['updateinterval'], fognode['hostname'], fognode['udpport'], fognode['tcpport'])
        for peer in fognode['peers']:
            shellscript += '\t\t{} {} \\\n'.format(peer['hostname'], peer['port'])
        shellscript = shellscript + '\t > logs/{}.{}.log 2>&1 &\n'.format(hostname, fognode['tcpport'])
    if firstHost:
        shellscript += '\tjava -jar IoTNodeReqGen/IoTNodeReqGen.jar testconfig.txt > logs/iotnodereqgen.{}.log 2>&1 &\n'.format(hostname)
    shellscript += '\tsleep {}\n'.format(sleepsecs)
    shellscript += '\tpkill -9 FogNode -U $UID\n'
    if firstHost:
        shellscript += '\tpkill -9 java -U $UID\n' 
    firstHost = False

shellscript += 'fi\n'
print shellscript

