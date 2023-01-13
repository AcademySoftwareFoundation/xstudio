// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <Imath/ImathBox.h>
#include <Imath/ImathMatrix.h>
#include <Imath/ImathVec.h>
#include "xstudio/utility/json_store.hpp"

// allow nlohmann serialisation of Imath::M44f
/*namespace nlohmann {
  template <typename T> struct adl_serializer<Imath::Matrix44<T>> {
      static void to_json(json &j, const Imath::Matrix44<T> &p) {
          j = json{
              "mat4",
              1,
              p[0][0],
              p[1][0],
              p[2][0],
              p[3][0],
              p[0][1],
              p[1][1],
              p[2][1],
              p[3][1],
              p[0][2],
              p[1][2],
              p[2][2],
              p[3][2],
              p[0][3],
              p[1][3],
              p[2][3],
              p[3][3]};
      }

      static void from_json(const json &j, Imath::Matrix44<T> &p) {
          auto vv = j.begin();
          vv++; // skip param type
          vv++; // skip count
          for (int i = 0; i < 4; ++i)
              for (int k = 0; k < 4; ++k)
                  p[i][j] = (vv++).value().get<T>();
      }
  };

  template <typename T> struct adl_serializer<Imath::Vec2<T>> {
      static void to_json(json &j, const Imath::Vec2<T> &p) { j = json{"vec2", 1, p.x, p.y}; }

      static void from_json(const json &j, Imath::Vec2<T> &p) {
          auto vv = j.begin();
          vv++; // skip param type
          vv++; // skip count
          p.x = (vv++).value().get<T>();
          p.y = (vv++).value().get<T>();
      }
  };

  template <typename T> struct adl_serializer<Imath::Vec3<T>> {
      static void to_json(json &j, const Imath::Vec3<T> &p) {
          j = json{"vec3", 1, p.x, p.y, p.z};
      }

      static void from_json(const json &j, Imath::Vec3<T> &p) {
          auto vv = j.begin();
          vv++; // skip param type
          vv++; // skip count
          p.x = (vv++).value().get<T>();
          p.y = (vv++).value().get<T>();
          p.z = (vv++).value().get<T>();
      }
  };

  template <typename T> struct adl_serializer<Imath::Vec4<T>> {
      static void to_json(json &j, const Imath::Vec4<T> &p) {
          j = json{"vec4", 1, p.x, p.y, p.z, p.w};
      }

      static void from_json(const json &j, Imath::Vec4<T> &p) {
          auto vv = j.begin();
          vv++; // skip param type
          vv++; // skip count
          p.x = (vv++).value().get<T>();
          p.y = (vv++).value().get<T>();
          p.z = (vv++).value().get<T>();
          p.w = (vv++).value().get<T>();
      }
  };

} // namespace nlohmann
*/