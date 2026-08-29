package com.example.mobileclock

import android.os.Bundle
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.activity.ComponentActivity

class MainActivity : ComponentActivity(), SurfaceHolder.Callback {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        System.loadLibrary("mobileclock")
        // Передаём доступ к app/src/main/assets нативному TTF-рендереру.
        nativeSetAssetManager(assets)

        val surfaceView = SurfaceView(this).apply {
            holder.addCallback(this@MainActivity)
            setOnTouchListener { _, event ->
                nativeTouch(event.actionMasked, event.x, event.y)
                true
            }
        }
        setContentView(surfaceView)
    }

    override fun surfaceCreated(holder: SurfaceHolder) = Unit

    override fun surfaceChanged(
        holder: SurfaceHolder,
        format: Int,
        width: Int,
        height: Int,
    ) {
        nativeSurfaceChanged(holder.surface, width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        nativeSurfaceDestroyed()
    }

    private external fun nativeSurfaceChanged(surface: Surface, width: Int, height: Int)
    private external fun nativeSetAssetManager(assetManager: android.content.res.AssetManager)
    private external fun nativeSurfaceDestroyed()
    private external fun nativeTouch(action: Int, x: Float, y: Float)
}
