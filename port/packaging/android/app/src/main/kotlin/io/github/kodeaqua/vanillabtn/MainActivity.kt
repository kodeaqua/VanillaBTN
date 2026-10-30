package io.github.kodeaqua.vanillabtn

import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Environment
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
            val dir = targetDataDir ?: File(getExternalFilesDir(null), "VanillaBTN")
            if (!dir.exists()) dir.mkdirs()
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

        log("MainActivity.onCreate: device=${Build.MANUFACTURER} ${Build.MODEL}, android=${Build.VERSION.RELEASE} (SDK ${Build.VERSION.SDK_INT})")

        // 1. Resolve secure app-private storage directory
        val safeDir = File(getExternalFilesDir(null), "VanillaBTN")
        if (!safeDir.exists()) safeDir.mkdirs()
        File(safeDir, "saves").mkdirs()

        val discCandidates = listOf(
            "game.bin", "hmbtn.bin", "VanillaBTN.bin",
            "game.cue", "hmbtn.cue", "VanillaBTN.cue"
        )

        // Prioritize safeDir disc file
        var candidateFile: File? = discCandidates.map { File(safeDir, it) }
            .firstOrNull { it.exists() && it.length() > 10 * 1024 * 1024 }

        targetDataDir = safeDir

        // 2. Fallback check in external Documents directory if safe storage is empty
        if (candidateFile == null) {
            val primaryDocs = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS)
            val primaryDir = File(primaryDocs, "VanillaBTN")
            val altDir = File(Environment.getExternalStorageDirectory(), "Documents/VanillaBTN")
            val fallbackDirs = listOf(primaryDir, altDir)

            for (dir in fallbackDirs) {
                if (!dir.exists()) continue
                candidateFile = discCandidates.map { File(dir, it) }.firstOrNull { it.exists() }
                if (candidateFile != null) {
                    if (isDirWritable(dir)) {
                        targetDataDir = dir
                    }
                    break
                }
            }
        }

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
            log("HMBTN_USER_DATA_DIR set to: ${targetDataDir!!.absolutePath}")
            Os.setenv("HMBTN_DISC_PATH", foundDiscFile!!.absolutePath, true)
            log("HMBTN_DISC_PATH set to: ${foundDiscFile!!.absolutePath}")
        } catch (e: Exception) {
            log("Failed to set environment variables: ${e.message}")
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

    private fun isDirWritable(dir: File): Boolean {
        if (!dir.exists() && !dir.mkdirs()) return false
        val testFile = File(dir, ".test_write_${System.currentTimeMillis()}")
        return try {
            if (testFile.createNewFile()) {
                testFile.delete()
                true
            } else {
                false
            }
        } catch (_: Exception) {
            false
        }
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
