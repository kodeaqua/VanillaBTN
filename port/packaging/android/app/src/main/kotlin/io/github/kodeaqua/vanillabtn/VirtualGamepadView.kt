package io.github.kodeaqua.vanillabtn

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import org.libsdl.app.SDLActivity
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.sin

class VirtualGamepadView(context: Context) : View(context) {

    private val density = resources.displayMetrics.density

    // Paint configurations
    private val basePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(80, 255, 255, 255)
        style = Paint.Style.STROKE
        strokeWidth = 3f * density
    }

    private val baseFillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(40, 0, 0, 0)
        style = Paint.Style.FILL
    }

    private val knobPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(180, 255, 255, 255)
        style = Paint.Style.FILL
    }

    private val buttonPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(70, 0, 0, 0)
        style = Paint.Style.FILL
    }

    private val buttonBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(120, 255, 255, 255)
        style = Paint.Style.STROKE
        strokeWidth = 2f * density
    }

    private val buttonPressedPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(140, 255, 255, 255)
        style = Paint.Style.FILL
    }

    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textSize = 18f * density
        textAlign = Paint.Align.CENTER
        isFakeBoldText = true
    }

    private val smallTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(200, 255, 255, 255)
        textSize = 12f * density
        textAlign = Paint.Align.CENTER
        isFakeBoldText = true
    }

    // Dynamic Analog Stick state
    private var stickActive = false
    private var stickPointerId = -1
    private var stickBaseX = 0f
    private var stickBaseY = 0f
    private var stickKnobX = 0f
    private var stickKnobY = 0f

    private val maxStickRadius = 55f * density
    private val deadzoneRadius = 14f * density
    private val baseCircleRadius = 55f * density
    private val knobRadius = 26f * density

    // Active D-pad keys triggered by analog stick
    private var activeDpadUp = false
    private var activeDpadDown = false
    private var activeDpadLeft = false
    private var activeDpadRight = false

    // Button definitions
    data class PadButton(
        val label: String,
        val keyCode: Int,
        var cx: Float = 0f,
        var cy: Float = 0f,
        var radius: Float = 28f,
        var isRect: Boolean = false,
        var rect: RectF = RectF(),
        var pressed: Boolean = false,
        var pointerId: Int = -1
    )

    private val buttons = mutableListOf<PadButton>()

    init {
        // PS1 Action Buttons
        buttons.add(PadButton("X", KeyEvent.KEYCODE_BUTTON_A))        // Cross (South)
        buttons.add(PadButton("O", KeyEvent.KEYCODE_BUTTON_B))        // Circle (East)
        buttons.add(PadButton("□", KeyEvent.KEYCODE_BUTTON_X))        // Square (West)
        buttons.add(PadButton("△", KeyEvent.KEYCODE_BUTTON_Y))        // Triangle (North)

        // Shoulders
        buttons.add(PadButton("L1", KeyEvent.KEYCODE_BUTTON_L1, isRect = true))
        buttons.add(PadButton("R1", KeyEvent.KEYCODE_BUTTON_R1, isRect = true))

        // System
        buttons.add(PadButton("SELECT", KeyEvent.KEYCODE_BUTTON_SELECT, isRect = true))
        buttons.add(PadButton("START", KeyEvent.KEYCODE_BUTTON_START, isRect = true))
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        layoutButtons(w.toFloat(), h.toFloat())
    }

    private fun layoutButtons(w: Float, h: Float) {
        val btnRadius = 28f * density
        val rightMargin = 85f * density
        val bottomMargin = 85f * density
        val btnCenterX = w - rightMargin
        val btnCenterY = h - bottomMargin
        val spacing = 46f * density

        for (btn in buttons) {
            btn.radius = btnRadius
            when (btn.keyCode) {
                KeyEvent.KEYCODE_BUTTON_A -> { // Cross (South)
                    btn.cx = btnCenterX
                    btn.cy = btnCenterY + spacing
                }
                KeyEvent.KEYCODE_BUTTON_B -> { // Circle (East)
                    btn.cx = btnCenterX + spacing
                    btn.cy = btnCenterY
                }
                KeyEvent.KEYCODE_BUTTON_X -> { // Square (West)
                    btn.cx = btnCenterX - spacing
                    btn.cy = btnCenterY
                }
                KeyEvent.KEYCODE_BUTTON_Y -> { // Triangle (North)
                    btn.cx = btnCenterX
                    btn.cy = btnCenterY - spacing
                }
                KeyEvent.KEYCODE_BUTTON_L1 -> {
                    btn.isRect = true
                    val rw = 70f * density
                    val rh = 34f * density
                    btn.rect = RectF(20f * density, 16f * density, 20f * density + rw, 16f * density + rh)
                }
                KeyEvent.KEYCODE_BUTTON_R1 -> {
                    btn.isRect = true
                    val rw = 70f * density
                    val rh = 34f * density
                    btn.rect = RectF(w - 20f * density - rw, 16f * density, w - 20f * density, 16f * density + rh)
                }
                KeyEvent.KEYCODE_BUTTON_SELECT -> {
                    btn.isRect = true
                    val rw = 60f * density
                    val rh = 26f * density
                    val cx = w * 0.42f
                    btn.rect = RectF(cx - rw / 2, 16f * density, cx + rw / 2, 16f * density + rh)
                }
                KeyEvent.KEYCODE_BUTTON_START -> {
                    btn.isRect = true
                    val rw = 60f * density
                    val rh = 26f * density
                    val cx = w * 0.58f
                    btn.rect = RectF(cx - rw / 2, 16f * density, cx + rw / 2, 16f * density + rh)
                }
            }
        }
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        // 1. Draw Dynamic Analog Stick if active
        if (stickActive) {
            // Base circle
            canvas.drawCircle(stickBaseX, stickBaseY, baseCircleRadius, baseFillPaint)
            canvas.drawCircle(stickBaseX, stickBaseY, baseCircleRadius, basePaint)
            // Knob
            canvas.drawCircle(stickKnobX, stickKnobY, knobRadius, knobPaint)
        }

        // 2. Draw Buttons
        for (btn in buttons) {
            if (btn.isRect) {
                val round = 8f * density
                canvas.drawRoundRect(btn.rect, round, round, if (btn.pressed) buttonPressedPaint else buttonPaint)
                canvas.drawRoundRect(btn.rect, round, round, buttonBorderPaint)
                val textY = btn.rect.centerY() - (smallTextPaint.descent() + smallTextPaint.ascent()) / 2
                canvas.drawText(btn.label, btn.rect.centerX(), textY, smallTextPaint)
            } else {
                canvas.drawCircle(btn.cx, btn.cy, btn.radius, if (btn.pressed) buttonPressedPaint else buttonPaint)
                canvas.drawCircle(btn.cx, btn.cy, btn.radius, buttonBorderPaint)
                val textY = btn.cy - (textPaint.descent() + textPaint.ascent()) / 2
                canvas.drawText(btn.label, btn.cx, textY, textPaint)
            }
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val action = event.actionMasked
        val actionIndex = event.actionIndex
        val pointerId = event.getPointerId(actionIndex)

        when (action) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val x = event.getX(actionIndex)
                val y = event.getY(actionIndex)

                // Check button hit first
                var hitButton = false
                for (btn in buttons) {
                    if (isHit(btn, x, y)) {
                        btn.pressed = true
                        btn.pointerId = pointerId
                        SDLActivity.onNativeKeyDown(btn.keyCode)
                        hitButton = true
                        invalidate()
                        break
                    }
                }

                if (!hitButton) {
                    // Check if in left half for dynamic analog stick
                    if (x < width * 0.45f && y > height * 0.20f && !stickActive) {
                        stickActive = true
                        stickPointerId = pointerId
                        stickBaseX = x
                        stickBaseY = y
                        stickKnobX = x
                        stickKnobY = y
                        invalidate()
                        return true
                    } else {
                        // Pass through to game view (for touching menus, dialogue, etc.)
                        return false
                    }
                }
                return true
            }

            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until event.pointerCount) {
                    val pid = event.getPointerId(i)
                    val px = event.getX(i)
                    val py = event.getY(i)

                    if (stickActive && pid == stickPointerId) {
                        updateAnalogStick(px, py)
                    } else {
                        // Track buttons
                        for (btn in buttons) {
                            if (btn.pointerId == pid) {
                                val stillHit = isHit(btn, px, py)
                                if (!stillHit && btn.pressed) {
                                    btn.pressed = false
                                    btn.pointerId = -1
                                    SDLActivity.onNativeKeyUp(btn.keyCode)
                                }
                            }
                        }
                    }
                }
                invalidate()
                return true
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_CANCEL -> {
                if (stickActive && pointerId == stickPointerId) {
                    resetAnalogStick()
                }

                for (btn in buttons) {
                    if (btn.pointerId == pointerId) {
                        if (btn.pressed) {
                            btn.pressed = false
                            SDLActivity.onNativeKeyUp(btn.keyCode)
                        }
                        btn.pointerId = -1
                    }
                }
                invalidate()
                return true
            }
        }
        return super.onTouchEvent(event)
    }

    private fun isHit(btn: PadButton, x: Float, y: Float): Boolean {
        return if (btn.isRect) {
            val expand = 8f * density
            x >= btn.rect.left - expand && x <= btn.rect.right + expand &&
            y >= btn.rect.top - expand && y <= btn.rect.bottom + expand
        } else {
            val dist = hypot(x - btn.cx, y - btn.cy)
            dist <= btn.radius + (8f * density)
        }
    }

    private fun updateAnalogStick(x: Float, y: Float) {
        val dx = x - stickBaseX
        val dy = y - stickBaseY
        val dist = hypot(dx, dy)

        if (dist > maxStickRadius) {
            stickKnobX = stickBaseX + (dx / dist) * maxStickRadius
            stickKnobY = stickBaseY + (dy / dist) * maxStickRadius
        } else {
            stickKnobX = x
            stickKnobY = y
        }

        if (dist > deadzoneRadius) {
            val angle = Math.toDegrees(atan2(dy.toDouble(), dx.toDouble()))
            // Map angle to D-Pad direction
            // -180..180: right=0, down=90, left=180, up=-90
            val up = angle in -157.5..-22.5
            val down = angle in 22.5..157.5
            val left = angle > 112.5 || angle < -112.5
            val right = angle in -67.5..67.5

            updateDpadKey(KeyEvent.KEYCODE_DPAD_UP, up, activeDpadUp) { activeDpadUp = it }
            updateDpadKey(KeyEvent.KEYCODE_DPAD_DOWN, down, activeDpadDown) { activeDpadDown = it }
            updateDpadKey(KeyEvent.KEYCODE_DPAD_LEFT, left, activeDpadLeft) { activeDpadLeft = it }
            updateDpadKey(KeyEvent.KEYCODE_DPAD_RIGHT, right, activeDpadRight) { activeDpadRight = it }
        } else {
            releaseAllDpad()
        }
    }

    private fun updateDpadKey(keyCode: Int, shouldBeActive: Boolean, current: Boolean, setter: (Boolean) -> Unit) {
        if (shouldBeActive != current) {
            if (shouldBeActive) {
                SDLActivity.onNativeKeyDown(keyCode)
            } else {
                SDLActivity.onNativeKeyUp(keyCode)
            }
            setter(shouldBeActive)
        }
    }

    private fun releaseAllDpad() {
        if (activeDpadUp) { SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_DPAD_UP); activeDpadUp = false }
        if (activeDpadDown) { SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_DPAD_DOWN); activeDpadDown = false }
        if (activeDpadLeft) { SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_DPAD_LEFT); activeDpadLeft = false }
        if (activeDpadRight) { SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_DPAD_RIGHT); activeDpadRight = false }
    }

    private fun resetAnalogStick() {
        stickActive = false
        stickPointerId = -1
        releaseAllDpad()
    }
}
