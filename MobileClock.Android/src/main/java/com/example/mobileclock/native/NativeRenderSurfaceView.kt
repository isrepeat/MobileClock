package com.example.mobileclock.native

import android.content.Context
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.Choreographer

class NativeRenderSurfaceView(context: Context) : SurfaceView(context), SurfaceHolder.Callback, Choreographer.FrameCallback {
    private var isRendering = false
    init {
        // Этот View — единственный Android-адаптер для Surface и touch-событий.
        holder.addCallback(this)
        setOnTouchListener { _, event ->
            NativeRenderer.onTouch(event.actionMasked, event.x, event.y)
            true
        }
    }

    override fun surfaceCreated(holder: SurfaceHolder) = Unit

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        // Surface из Android передаётся через Kotlin JNI-фасаду, затем в C++
        // преобразуется в ANativeWindow* для создания EGLSurface.
        NativeRenderer.onSurfaceChanged(holder.surface, width, height)
        isRendering = true
        Choreographer.getInstance().postFrameCallback(this)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        isRendering = false
        NativeRenderer.onSurfaceDestroyed()
    }

    override fun doFrame(frameTimeNanos: Long) {
        if (!isRendering) return
        NativeRenderer.render()
        Choreographer.getInstance().postFrameCallback(this)
    }
}