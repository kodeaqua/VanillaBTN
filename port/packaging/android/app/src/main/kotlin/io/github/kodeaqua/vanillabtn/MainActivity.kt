package io.github.kodeaqua.vanillabtn

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.system.Os
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.appcompat.app.AlertDialog
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.libsdl.app.SDLActivity
import java.io.File

class MainActivity : SDLActivity() {

    private var foundDiscFile: File? = null
    private var gamepadOverlay: VirtualGamepadView? = null

    override fun getLibraries(): Array<String> {
        return arrayOf(
            "SDL3",
            "VanillaBTN"
        )
    }

    override fun getMainSharedObject(): String {
        return "libVanillaBTN.so"
    }

    override fun getMainFunction(): String {
        return "SDL_main"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        // Enforce sticky immersive full screen
        WindowCompat.setDecorFitsSystemWindows(window, false)
        val controller = WindowInsetsControllerCompat(window, window.decorView)
        controller.hide(WindowInsetsCompat.Type.systemBars())
        controller.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE

        // Check storage directory
        val docsDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS)
        val vanillaDir = File(docsDir, "VanillaBTN")
        if (!vanillaDir.exists()) {
            vanillaDir.mkdirs()
        }

        try {
            Os.setenv("HMBTN_USER_DATA_DIR", vanillaDir.absolutePath, true)
        } catch (_: Exception) {}

        // Scan for game disc (.bin / .cue)
        val candidates = listOf(
            "game.cue", "game.bin",
            "hmbtn.cue", "hmbtn.bin",
            "VanillaBTN.cue", "VanillaBTN.bin"
        )
        foundDiscFile = candidates.map { File(vanillaDir, it) }.firstOrNull { it.exists() }
        if (foundDiscFile == null) {
            foundDiscFile = vanillaDir.listFiles()?.firstOrNull { file ->
                val name = file.name.lowercase()
                (name.endsWith(".bin") || name.endsWith(".cue")) && !name.endsWith(".hmc") && !name.endsWith(".tmp")
            }
        }

        if (foundDiscFile != null) {
            try {
                Os.setenv("HMBTN_DISC_PATH", foundDiscFile!!.absolutePath, true)
            } catch (_: Exception) {}
        }

        super.onCreate(savedInstanceState)

        // If game disc is missing, show Material 3 Expressive Dialog immediately
        if (foundDiscFile == null) {
            showMissingDiscDialog(vanillaDir.absolutePath)
        } else {
            // Attach Virtual Gamepad Overlay over SDL Surface
            setupGamepadOverlay()
        }
    }

    override fun getArguments(): Array<String> {
        return if (foundDiscFile != null) {
            arrayOf(foundDiscFile!!.absolutePath)
        } else {
            emptyArray()
        }
    }

    private fun showMissingDiscDialog(folderPath: String) {
        val dialog = MaterialAlertDialogBuilder(this)
            .setTitle(R.string.game_disc_not_found_title)
            .setMessage(getString(R.string.game_disc_not_found_message))
            .setCancelable(false)
            .setPositiveButton(R.string.action_exit) { _, _ ->
                finish()
            }
            .create()

        dialog.show()
    }

    private fun setupGamepadOverlay() {
        val overlay = VirtualGamepadView(this)
        gamepadOverlay = overlay
        val params = FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT
        )
        mLayout.addView(overlay, params)
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
