ROOT=$(cd "$(dirname "$0")/.."; pwd)

# Resave jucer files
"$ROOT/build/bin/JUCE/Projucer.app/Contents/MacOS/Projucer" --resave "$ROOT/gs1.jucer"

cd "$ROOT/Builds/MacOSX"
xcodebuild -configuration Release || exit 1
