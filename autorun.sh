export LD_LIBRARY_PATH=/var/lib
export PATH=${PATH}:/var/bin:/var/plugins

echo "### Starting NEUTRINO ###"

cd /tmp
/bin/neutrino > /dev/null 2> /dev/null

/bin/sync
/bin/sync

if [ -e /tmp/.reboot ] ; then
    /bin/dt -t"Rebooting..."
    /sbin/reboot -f
else
    /bin/dt -t"Panic..."
    sleep 5
    /sbin/reboot -f
fi
