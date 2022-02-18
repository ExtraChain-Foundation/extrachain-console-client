# tested on ubuntu 18.04
# example: extrachain-make-archive.sh /path/to/Qt/not/bin /path/to/cmake/build

echo "Copy libs..."
cqtdeployer -bin $2/extrachain-console -qmake $1/bin/qmake -fileLog cqtdeployer.log -verbose 0
cp $2/libextrachain.so DistributionKit/lib/libextrachain.so
cp /usr/lib/x86_64-linux-gnu/libgmp.so.10 DistributionKit/lib/libgmp.so.10
echo "Done"

echo "Generate archive..."
mv DistributionKit ExtraChainConsole
tar cfz ExtraChainConsole.tar.gz ExtraChainConsole
rm -rf ExtraChainConsole
rm cqtdeployer.log

echo "Generated ExtraChainConsole.tar.gz"