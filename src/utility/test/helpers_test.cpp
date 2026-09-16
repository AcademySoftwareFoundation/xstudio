// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/utility/serialise_headers.hpp"


#include "xstudio/utility/container.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/timecode.hpp"
#include "xstudio/utility/types.hpp"

using namespace caf;
using namespace xstudio::utility;

ACTOR_TEST_SETUP()

// An absolute path prefix that is absolute on the platform under test.
// '/file.mov' is drive-less on Windows, so it is not one there.
#ifdef _WIN32
#define ABS_ "C:/"
#else
#define ABS_ "/"
#endif

namespace {

// Compare paths without asserting a slash convention. A leading '\\' is the
// exception - that is the UNC marker these tests care about.
std::string fwd(std::string p) {
    std::replace(p.begin(), p.end(), '\\', '/');
    return p;
}

caf::uri uri_of(const std::string &s) {
    auto u = caf::make_uri(s);
    EXPECT_TRUE(u) << "could not parse uri: " << s;
    return u ? *u : caf::uri();
}

} // namespace


TEST(HelpersTest, Test) {
    FrameList fl;

    EXPECT_EQ(posix_path_to_uri(ABS_ "file.mov", true), parse_cli_posix_path(ABS_ "file.mov", fl));

    EXPECT_EQ(to_string(posix_path_to_uri("file.exr")), "file:file.exr");


    EXPECT_EQ(uri_to_posix_path(posix_path_to_uri("file.exr")), "file.exr");

    EXPECT_THROW(parse_cli_posix_path("file.{:04d}.exr", fl), std::runtime_error)
        << "Should be exception";

    EXPECT_THROW(parse_cli_posix_path("file.{:04d}.exr", fl, true), std::runtime_error)
        << "Should be exception";

    EXPECT_EQ(
        posix_path_to_uri(ABS_ "file.{:04d}.exr", true),
        parse_cli_posix_path(ABS_ "file.{:04d}.exr=1-10", fl));
    EXPECT_EQ("1-10", to_string(fl));

    EXPECT_EQ(
        posix_path_to_uri(ABS_ "file.{:04d}.exr", true),
        parse_cli_posix_path(ABS_ "file.1-10{:04d}.exr", fl));
    EXPECT_EQ("1-10", to_string(fl));

    EXPECT_EQ(
        posix_path_to_uri(ABS_ "file.{:04d}.exr", true),
        parse_cli_posix_path(ABS_ "file.1-10#.exr", fl));
    EXPECT_EQ("1-10", to_string(fl));

    EXPECT_EQ(
        posix_path_to_uri(ABS_ "file.{:04d}.exr", true),
        parse_cli_posix_path(ABS_ "file.@@@@.exr=1-10", fl));
    EXPECT_EQ("1-10", to_string(fl));

    EXPECT_EQ(
        posix_path_to_uri(ABS_ "file.{:04d}.exr", true),
        parse_cli_posix_path(ABS_ "file.#.exr=1-10", fl));
    EXPECT_EQ("1-10", to_string(fl));

    EXPECT_EQ(
        posix_path_to_uri(ABS_ "file.{:03d}.exr", true),
        parse_cli_posix_path(ABS_ "file.###.exr=1-10", fl));
    EXPECT_EQ("1-10", to_string(fl));

    EXPECT_EQ(
        posix_path_to_uri(TEST_RESOURCE "/media/test.{:04d}.ppm", true),
        parse_cli_posix_path(TEST_RESOURCE "/media/test.####.ppm", fl, true));
    EXPECT_EQ("1-10", to_string(fl));

    // EXPECT_EQ(
    //     uri_to_posix_path(url_to_uri(
    //         "file://localhost/user_data/test_files/demo_files/Beasts%20Of%20Burden/03.mov")),
    //     "/user_data/test_files/demo_files/Beasts Of Burden/03.mov");

    // EXPECT_EQ(
    //     uri_to_posix_path(
    //         url_to_uri("file:/user_data/test_files/demo_files/Beasts%20Of%20Burden/03.mov")),
    //     "/user_data/test_files/demo_files/Beasts Of Burden/03.mov");

    // EXPECT_EQ(
    //     uri_to_posix_path(
    //         url_to_uri("file:///user_data/test_files/demo_files/Beasts%20Of%20Burden/03.mov")),
    //     "/user_data/test_files/demo_files/Beasts Of Burden/03.mov");
}


TEST(Helpers2Test, Test) {
#ifdef _WIN32
    // There is no HOME on Windows - getenv returns null and constructing a
    // std::string from it is undefined. expand_envvars resolves ${HOME}
    // through ${USERPROFILE} there, so that is what it should come back as.
    EXPECT_EQ(expand_envvars("${HOME}"), std::getenv("USERPROFILE"));
    EXPECT_EQ(
        " " + expand_envvars("${HOME}") + " ",
        std::string(" ") + std::getenv("USERPROFILE") + " ");
#else
    EXPECT_EQ(expand_envvars("${HOME}"), std::getenv("HOME"));
    EXPECT_EQ(
        " " + expand_envvars("${HOME}") + " ", std::string(" ") + std::getenv("HOME") + " ");
#endif
}


TEST(UriToPosixPathSchemeTest, WebUrisDoNotBecomeHostPaths) {
    struct Case {
        const char *url;
        const char *host;
    };

    // http and https are the only non-file schemes xstudio takes media from,
    // see playlist_actor.cpp.
    const Case cases[] = {
        {"https://host.example.com/a/b.mp4", "host.example.com"},
        {"http://host.example.com/a/b.mp4", "host.example.com"},
        {"http://host.example.com:8080/a/b.mp4", "host.example.com"},
        // A query string must not end up in the path, or in the extension.
        {"https://host.example.com/a/b.mp4?expires=86400&signature=abc123", "host.example.com"},
    };

    for (const auto &c : cases) {
        const auto path = uri_to_posix_path(uri_of(c.url));

        EXPECT_NE(path.rfind("\\\\", 0), 0u)
            << c.url << " became a UNC path: " << path;
        EXPECT_EQ(path.find(c.host), std::string::npos)
            << c.url << " leaked the host into the path: " << path;
        EXPECT_EQ(get_path_extension(fs::path(path)), ".mp4") << c.url;
    }

    // What a web uri does come back as: the path component only. Nothing
    // downstream should hand this to the filesystem, but is_file_supported()
    // and media_actor both read the extension off it to pick a reader, so it
    // cannot simply be empty.
    EXPECT_EQ(uri_to_posix_path(uri_of("https://host.example.com/a/b.mp4")), "/a/b.mp4");
}

TEST(UriToPosixPathSchemeTest, FileUrisKeepTheirPathSemantics) {
#ifdef _WIN32
    EXPECT_EQ(
        uri_to_posix_path(uri_of("file://server/share/path.mov")),
        "\\\\server\\share\\path.mov");
    EXPECT_EQ(
        uri_to_posix_path(uri_of("file://server////share/x.mov")),
        "\\\\server\\share\\x.mov");
    EXPECT_EQ(fwd(uri_to_posix_path(uri_of("file://localhost/C:/media/a.mov"))), "C:/media/a.mov");
    EXPECT_EQ(fwd(uri_to_posix_path(uri_of("file:///C:/media/a.mov"))), "C:/media/a.mov");
#else
    EXPECT_EQ(uri_to_posix_path(uri_of("file:///media/a.mov")), "/media/a.mov");
    EXPECT_EQ(uri_to_posix_path(uri_of("file:/media/a.mov")), "/media/a.mov");
#endif
}

#ifdef _WIN32
TEST(UriToPosixPathSchemeTest, DriveLetterForms) {
    EXPECT_EQ(fwd(uri_to_posix_path(uri_of("file:///C:/media/a.mov"))), "C:/media/a.mov");
    EXPECT_EQ(fwd(uri_to_posix_path(uri_of("file:///c:/media/a.mov"))), "c:/media/a.mov");
    EXPECT_EQ(fwd(uri_to_posix_path(uri_of("file:///Z:/media/a.mov"))), "Z:/media/a.mov");
    // A mapped share is just a drive letter by the time it gets here.
    EXPECT_EQ(fwd(uri_to_posix_path(uri_of("file:///C:/"))), "C:/");
}

TEST(UriToPosixPathSchemeTest, UncHostForms) {
    EXPECT_EQ(
        uri_to_posix_path(uri_of("file://server.domain.com/share/f.mov")),
        "\\\\server.domain.com\\share\\f.mov");
    EXPECT_EQ(
        uri_to_posix_path(uri_of("file://192.168.0.1/share/f.mov")),
        "\\\\192.168.0.1\\share\\f.mov");
    // Share root, no file.
    EXPECT_EQ(uri_to_posix_path(uri_of("file://server/share")), "\\\\server\\share");
}
#endif

TEST(UriToPosixPathSchemeTest, RoundTrip) {
    const char *paths[] = {
        ABS_ "media/a.mov",
        ABS_ "media/with space/b.exr",
        ABS_ "media/seq.{:04d}.exr",
        ABS_ "media/paren(1).mov",
        ABS_ "media/comma,name.mov",
        "relative.mov",
#ifdef _WIN32
        "C:\\media\\backslash.mov",
        "\\\\server\\share\\c.mov",
        "\\\\server.domain.com\\share\\with space\\d.mov",
#endif
    };

    for (const auto *p : paths) {
        EXPECT_EQ(fwd(uri_to_posix_path(posix_path_to_uri(p))), fwd(p)) << "round trip: " << p;
    }
}

// Characters that are significant in a URI but legal in a filename on every
// platform, so they have to survive the encode/decode round trip.
TEST(UriToPosixPathSchemeTest, RoundTripReservedCharacters) {
    const char *paths[] = {
        ABS_ "media/hash#name.mov",
        ABS_ "media/plus+name.mov",
        ABS_ "media/amp&name.mov",
        ABS_ "media/at@name.mov",
        ABS_ "media/semi;name.mov",
        ABS_ "media/quote'name.mov",
        ABS_ "media/equals=name.mov",
        ABS_ "media/bracket[1].mov",
        ABS_ "media/caret^name.mov",
        ABS_ "media/dollar$name.mov",
        ABS_ "media/tilde~name.mov",
        ABS_ "media/excl!name.mov",
    };

    for (const auto *p : paths) {
        EXPECT_EQ(fwd(uri_to_posix_path(posix_path_to_uri(p))), fwd(p)) << "round trip: " << p;
    }
}

// Windows forbids < > : " / \ | ? * in a filename, so these forms can only
// exist on POSIX.
#ifndef _WIN32
TEST(UriToPosixPathSchemeTest, RoundTripPosixOnlyCharacters) {
    const char *paths[] = {
        "/media/question?name.mov",
        "/media/star*name.mov",
        "/media/dquote\"name.mov",
        "/media/lt<name.mov",
        "/media/gt>name.mov",
        "/media/pipe|name.mov",
        "/media/colon:name.mov",
    };

    for (const auto *p : paths) {
        EXPECT_EQ(fwd(uri_to_posix_path(posix_path_to_uri(p))), fwd(p)) << "round trip: " << p;
    }
}
#endif

// A literal '%' is deliberately not covered here: it does not survive the
// round trip, which is a pre-existing bug in the encode/decode pair rather
// than anything to do with the authority handling above.
