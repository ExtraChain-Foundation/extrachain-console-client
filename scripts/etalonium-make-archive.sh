# tested on ubuntu 18.04
# example: etalonium-make-archive.sh /path/to/Qt/not/bin /path/to/cmake/build

echo "Copy libs..."
cqtdeployer -bin $2/etalonium-console -qmake $1/bin/qmake -fileLog cqtdeployer.log -verbose 0
cp $2/libextrachain.so DistributionKit/lib/libextrachain.so
cp /usr/lib/x86_64-linux-gnu/libgmp.so.10 DistributionKit/lib/libgmp.so.10
echo "Done"

echo "Generate archive..."
mv DistributionKit EtaloniumConsole
tar cfz EtaloniumConsole.tar.gz EtaloniumConsole
rm -rf EtaloniumConsole
rm cqtdeployer.log

echo "Generated EtaloniumConsole.tar.gz"