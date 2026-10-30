package io.github.kodeaqua.vanillabtn

import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.system.Os
import android.util.Log
import android.view.KeyEvent
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import org.libsdl.app.SDLActivity
import java.io.File
import java.io.FileWriter
import java.io.PrintWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class MainActivity : SDLActivity() {

    private var foundDiscFile: File? = null
    private var targetDataDir: File? = null
    private var gamepadOverlay: VirtualGamepadView? = null

    override fun getLibraries(): Array<String> {
        return arrayOf("VanillaBTN")
    }

    override fun getMainFunction(): String {
        return "SDL_main"
    }

    private fun log(message: String) {
        Log.i("VanillaBTN", message)
        try {
            val dir = targetDataDir ?: return
            val logFile = File(dir, "last_run.log")
            val time = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(Date())
            FileWriter(logFile, true).use { fw ->
                PrintWriter(fw).use { pw ->
                    pw.println("[$time] $message")
                }
            }
        } catch (_: Exception) {}
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        // Enforce sticky immersive full screen
        WindowCompat.setDecorFitsSystemWindows(window, false)
        val controller = WindowInsetsControllerCompat(window, window.decorView)
        controller.hide(WindowInsetsCompat.Type.systemBars())
        controller.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE

        // Resolve app-private storage before SDL starts native code.
        val safeDir = try {
            AppStorage.ensureUserDataDir(this)
        } catch (_: Exception) {
            log("App-private storage is unavailable")
            startActivity(Intent(this, LauncherActivity::class.java))
            finish()
            super.onCreate(savedInstanceState)
            return
        }
        targetDataDir = safeDir
        log("MainActivity.onCreate: device=${Build.MANUFACTURER} ${Build.MODEL}, android=${Build.VERSION.RELEASE} (SDK ${Build.VERSION.SDK_INT})")

        val discCandidates = listOf(
            "game.bin", "hmbtn.bin", "VanillaBTN.bin"
        )

        // Prioritize safeDir disc file
        var candidateFile: File? = discCandidates.map { File(safeDir, it) }
            .firstOrNull { it.isFile && it.length() > 10 * 1024 * 1024 }

        foundDiscFile = candidateFile

        // If no disc file found anywhere, redirect to LauncherActivity setup
        if (foundDiscFile == null) {
            log("No game disc found, redirecting to LauncherActivity setup screen.")
            val intent = Intent(this, LauncherActivity::class.java)
            startActivity(intent)
            finish()
            super.onCreate(savedInstanceState)
            return
        }

        try {
            Os.setenv("HMBTN_USER_DATA_DIR", targetDataDir!!.absolutePath, true)
            Os.setenv("HMBTN_DISC_PATH", foundDiscFile!!.absolutePath, true)
            log("Native storage and disc paths configured")
        } catch (_: Exception) {
            log("Could not configure native storage and disc paths")
        }

        super.onCreate(savedInstanceState)

        // Attach Virtual Gamepad Overlay over SDL Surface
        setupGamepadOverlay()
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (event.keyCode == KeyEvent.KEYCODE_BACK) {
            if (event.action == KeyEvent.ACTION_DOWN) {
                onNativeKeyDown(KeyEvent.KEYCODE_ESCAPE)
            } else if (event.action == KeyEvent.ACTION_UP) {
                onNativeKeyUp(KeyEvent.KEYCODE_ESCAPE)
            }
            return true
        }
        return super.dispatchKeyEvent(event)
    }

    @Suppress("DEPRECATION")
    override fun onBackPressed() {
        onNativeKeyDown(KeyEvent.KEYCODE_ESCAPE)
        onNativeKeyUp(KeyEvent.KEYCODE_ESCAPE)
    }

    override fun getArguments(): Array<String> {
        return if (foundDiscFile != null) {
            arrayOf(foundDiscFile!!.absolutePath)
        } else {
            emptyArray()
        }
    }

    private fun setupGamepadOverlay() {
        val layout = mLayout ?: return
        val overlay = VirtualGamepadView(this)
        gamepadOverlay = overlay
        val params = FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT
        )
        layout.addView(overlay, params)
        log("VirtualGamepadView overlay attached successfully")
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            val controller = WindowInsetsControllerCompat(window, window.decorView)
            controller.hide(WindowInsetsCompat.Type.systemBars())
            controller.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }
}
