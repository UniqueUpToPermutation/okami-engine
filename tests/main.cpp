#include <gtest/gtest.h>

#include <glog/logging.h>

int main(int argc, char** argv) {
    google::InitGoogleLogging(argv[0]);
    FLAGS_minloglevel = google::GLOG_WARNING; // Suppress INFO-level logs in tests
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}