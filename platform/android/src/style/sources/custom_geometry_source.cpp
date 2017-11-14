#include "custom_geometry_source.hpp"

#include <mbgl/renderer/query.hpp>

// Java -> C++ conversion
#include "../android_conversion.hpp"
#include "../conversion/filter.hpp"

// C++ -> Java conversion
#include "../../conversion/conversion.hpp"
#include "../../conversion/collection.hpp"
#include "../../geojson/conversion/feature.hpp"
#include <mbgl/style/conversion/custom_geometry_source_options.hpp>

#include <string>

namespace mbgl {
namespace android {
    struct SourceWrapper {
        std::shared_ptr<CustomGeometrySource> source;
    };

    // This conversion is expected not to fail because it's used only in contexts where
    // the value was originally a GeoJsonOptions object on the Java side. If it fails
    // to convert, it's a bug in our serialization or Java-side static typing.
    static style::CustomGeometrySource::Options convertCustomGeometrySourceOptions(jni::JNIEnv& env,
                                                                                   jni::Object<> options,
                                                                                   style::TileFunction fetchFn,
                                                                                   style::TileFunction cancelFn) {
        using namespace mbgl::style::conversion;
        if (!options) {
            return style::CustomGeometrySource::Options();
        }
        Error error;
        optional<style::CustomGeometrySource::Options> result = convert<style::CustomGeometrySource::Options>(Value(env, options), error);
        if (!result) {
            throw std::logic_error(error.message);
        }
        result->fetchTileFunction = fetchFn;
        result->cancelTileFunction = cancelFn;
        return *result;
    }

    CustomGeometrySource::CustomGeometrySource(jni::JNIEnv& env,
                                               jni::Object<CustomGeometrySource> _obj,
                                               jni::String sourceId,
                                               jni::Object<> options)
        : Source(env, std::make_unique<mbgl::style::CustomGeometrySource>(
                         jni::Make<std::string>(env, sourceId),
                         convertCustomGeometrySourceOptions(env, options,
                                 std::bind(&CustomGeometrySource::fetchTile, this, std::placeholders::_1),
                                 std::bind(&CustomGeometrySource::cancelTile, this, std::placeholders::_1)))),
          weakJavaPeer(
                  SeizeGenericWeakRef(env, jni::Object<CustomGeometrySource>(jni::NewWeakGlobalRef(env, _obj.Get()).release()))) {
    }

    CustomGeometrySource::~CustomGeometrySource() {
        // Before being added to a map, the Java peer owns this C++ peer and cleans
        //  up after itself correctly through the jni native peer bindings.
        // After being added to the map, the ownership is flipped and the C++ peer has a strong reference
        //  to it's Java peer, preventing the Java peer from being GC'ed.
        //  In this case, the core source intiaites the destruction, which requires releasing the Java peer,
        //  while also resetting it's nativePtr to 0 to prevent the subsequent GC of the Java peer from
        // re-entering this dtor.
        if (ownedSource.get() == nullptr && javaPeer.get() != nullptr) {
            //Manually clear the java peer
            android::UniqueEnv env = android::AttachEnv();

            static auto clearRequests = javaClass.GetMethod<void()>(*env, "clear");
            javaPeer->Call(*env, clearRequests);

            static auto nativePtrField = javaClass.GetField<jlong>(*env, "nativePtr");
            javaPeer->Set(*env, nativePtrField, (jlong) 0);

            javaPeer.reset();
        }
    };

    void CustomGeometrySource::addToMap(mbgl::Map& map) {
        Source::addToMap(map);

        // When adding a CustomGeometrySource to the map, flip the ownership and control.
        // Before adding to the map:
        // - the Java peer owns this C++ Peer through the nativePtr field
        // - this C++ peer has a weak reference to the java peer
        // - this C++ peer owns the core source in a unique_ptr
        // After adding to the map:
        // - this C++ peer converts its weak reference to the Java peer to a strong/Global reference
        // - the C++ peer is owned by the core source's peer member
        android::UniqueEnv _env = android::AttachEnv();
        javaPeer = weakJavaPeer->NewGlobalRef(*_env);
        weakJavaPeer.reset();
    }

    void CustomGeometrySource::fetchTile (const mbgl::CanonicalTileID& tileID) {
        android::UniqueEnv _env = android::AttachEnv();
        static auto fetchTile = javaClass.GetMethod<void (jni::jint, jni::jint, jni::jint)>(*_env, "fetchTile");
        assert(javaPeer);
        javaPeer->Call(*_env, fetchTile, (int)tileID.z, (int)tileID.x, (int)tileID.y);
    };

    void CustomGeometrySource::cancelTile(const mbgl::CanonicalTileID& tileID) {
        android::UniqueEnv _env = android::AttachEnv();
        static auto cancelTile = javaClass.GetMethod<void (jni::jint, jni::jint, jni::jint)>(*_env, "cancelTile");
        assert(javaPeer);
        javaPeer->Call(*_env, cancelTile, (int)tileID.z, (int)tileID.x, (int)tileID.y);
    };

    void CustomGeometrySource::setTileData(jni::JNIEnv& env,
                                           jni::jint z,
                                           jni::jint x,
                                           jni::jint y,
                                           jni::Object<geojson::FeatureCollection> jFeatures) {
        using namespace mbgl::android::geojson;

        // Convert the jni object
        auto geometry = geojson::FeatureCollection::convert(env, jFeatures);

        // Update the core source
        source.as<mbgl::style::CustomGeometrySource>()->CustomGeometrySource::setTileData(CanonicalTileID(z, x, y), GeoJSON(geometry));
    }

    void CustomGeometrySource::invalidateTile(jni::JNIEnv&, jni::jint z, jni::jint x, jni::jint y) {
        source.as<mbgl::style::CustomGeometrySource>()->CustomGeometrySource::invalidateTile(CanonicalTileID(z, x, y));
    }

    void CustomGeometrySource::invalidateBounds(jni::JNIEnv& env, jni::Object<LatLngBounds> jBounds) {
        auto bounds = LatLngBounds::getLatLngBounds(env, jBounds);
        source.as<mbgl::style::CustomGeometrySource>()->CustomGeometrySource::invalidateRegion(bounds);
    }

    jni::Array<jni::Object<geojson::Feature>> CustomGeometrySource::querySourceFeatures(jni::JNIEnv& env,
                                                                        jni::Array<jni::Object<>> jfilter) {
        using namespace mbgl::android::conversion;
        using namespace mbgl::android::geojson;

        std::vector<mbgl::Feature> features;
        if (rendererFrontend) {
            features = rendererFrontend->querySourceFeatures(source.getID(), { {},  toFilter(env, jfilter) });
        }
        return *convert<jni::Array<jni::Object<Feature>>, std::vector<mbgl::Feature>>(env, features);
    }

    jni::Class<CustomGeometrySource> CustomGeometrySource::javaClass;

    jni::jobject* CustomGeometrySource::createJavaPeer(jni::JNIEnv& env) {
        static auto constructor = CustomGeometrySource::javaClass.template GetConstructor<jni::jlong>(env);
        return CustomGeometrySource::javaClass.New(env, constructor, reinterpret_cast<jni::jlong>(this));
    }

    void CustomGeometrySource::registerNative(jni::JNIEnv& env) {
        // Lookup the class
        CustomGeometrySource::javaClass = *jni::Class<CustomGeometrySource>::Find(env).NewGlobalRef(env).release();
        static auto nativePtrField = javaClass.GetField<jlong>(env, "nativePtr");

        #define METHOD(MethodPtr, name) jni::MakeNativePeerMethod<decltype(MethodPtr), (MethodPtr)>(name)

        // Register the peer instance methods
        jni::RegisterNativePeer<CustomGeometrySource>(
            env, CustomGeometrySource::javaClass, "nativePtr",
            METHOD(&CustomGeometrySource::querySourceFeatures, "querySourceFeatures"),
            METHOD(&CustomGeometrySource::setTileData, "nativeSetTileData"),
            METHOD(&CustomGeometrySource::invalidateTile, "nativeInvalidateTile"),
            METHOD(&CustomGeometrySource::invalidateBounds, "nativeInvalidateBounds")
        );

        auto initialize = [](jni::JNIEnv& env, jni::Object<CustomGeometrySource> javaSource, jni::String sourceId,
                             jni::Object<> options) {
            auto sharedSource = std::make_shared<CustomGeometrySource>(env, javaSource, sourceId, options);
            javaSource.Set(env, nativePtrField, reinterpret_cast<jni::jlong>(sharedSource.get()));
            sharedSource->source.peer = SourceWrapper { sharedSource->shared_from_this() };
        };

        auto finalize = [](jni::JNIEnv& env, jni::Object<CustomGeometrySource> javaSource) {
            auto sharedSource = std::shared_ptr<CustomGeometrySource>(reinterpret_cast<CustomGeometrySource *>((jlong)javaSource.Get(env, nativePtrField)));
            if (sharedSource) javaSource.Set(env, nativePtrField, (jlong)0);
            sharedSource.reset();
        };

        //Register the peer ctor and dtor helpers
        jni::RegisterNatives(
            env, CustomGeometrySource::javaClass,
            jni::MakeNativeMethod("initialize", initialize),
            jni::MakeNativeMethod("finalize", finalize)
        );
    }

} // namespace android
} // namespace mbgl
