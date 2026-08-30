# Android platform layer

This layer owns JNI entry points, `ANativeWindow`, Android assets, and lifecycle
translation. It is the only native layer allowed to include Android JNI headers.
