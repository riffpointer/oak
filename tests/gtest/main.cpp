#include <gtest/gtest.h>
#include <QGuiApplication>

#ifdef Q_OS_MAC
#include <QProcessEnvironment>
#endif

int main(int argc, char **argv)
{
#ifdef Q_OS_MAC
    // macOS: default to 'minimal' platform to avoid random crashes caused by
    // NSApplication autorelease pool conflicts with FFmpeg's atempo filter.
    // Users can override with QT_QPA_PLATFORM=cocoa to run GPU tests locally.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "minimal");
    }
#endif
    QGuiApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
