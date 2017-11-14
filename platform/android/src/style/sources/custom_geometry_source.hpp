#pragma once

#include "source.hpp"
#include <mbgl/style/sources/custom_geometry_source.hpp>
#include <mbgl/util/geojson.hpp>
#include <mbgl/tile/tile_id.hpp>
#include "../../geojson/geometry.hpp"
#include "../../geojson/feature.hpp"
#include "../../geojson/feature_collection.hpp"
#include "../../geometry/lat_lng_bounds.hpp"
#include <jni/jni.hpp>

namespace mbgl {
namespace android {

class CustomGeometrySource : public Source, public std::enable_shared_from_this<CustomGeometrySource> {
public:

    static constexpr auto Name() { return "com/mapbox/mapboxsdk/style/sources/CustomGeometrySource"; };

    static jni::Class<CustomGeometrySource> javaClass;

    static void registerNative(jni::JNIEnv&);

    CustomGeometrySource(jni::JNIEnv&,
                       jni::Object<CustomGeometrySource>,
                       jni::String,
                       jni::Object<>);

    ~CustomGeometrySource();

    void addToMap(mbgl::Map&) override;

    void fetchTile(const mbgl::CanonicalTileID& tileID);
    void cancelTile(const mbgl::CanonicalTileID& tileID);
    void setTileData(jni::JNIEnv& env, jni::jint z, jni::jint x, jni::jint y, jni::Object<geojson::FeatureCollection> jf);

    void invalidateTile(jni::JNIEnv& env, jni::jint z, jni::jint x, jni::jint y);
    void invalidateBounds(jni::JNIEnv& env, jni::Object<LatLngBounds> bounds);

    jni::Array<jni::Object<geojson::Feature>> querySourceFeatures(jni::JNIEnv&,
                                                                  jni::Array<jni::Object<>> );

    jni::jobject* createJavaPeer(jni::JNIEnv&) override;

    // Use a strong global reference to enable fetch/cancel methods to call java peer from any thread
    jni::UniqueObject<CustomGeometrySource> javaPeer;

    // Use a weak reference to the peer until the source is added to the map.
    GenericUniqueWeakObject<CustomGeometrySource> weakJavaPeer;

}; // class CustomGeometrySource

} // namespace android
} // namespace mbgl
