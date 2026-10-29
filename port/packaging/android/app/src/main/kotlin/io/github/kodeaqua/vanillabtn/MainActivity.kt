package io.github.kodeaqua.vanillabtn

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import android.system.Os
import android.view.ViewGroup
import android.widget.FrameLayout
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
    private var targetDataDir: File? = null
    private var gamepadOverlay: VirtualGamepadView? = null

    override fun getLibraries(): Array<String> {
        // SDL3 is statically bundled inside libVanillaBTN.so
        return arrayOf("VanillaBTN")
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

        // Request storage permissions if needed
        checkStoragePermissions()

        // Resolve user data directory (Internal Storage -> Documents/VanillaBTN)
        val primaryDocs = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS)
        val primaryDir = File(primaryDocs, "VanillaBTN")
        val altDir = File(Environment.getExternalStorageDirectory(), "Documents/VanillaBTN")
        val appSpecificDir = getExternalFilesDir(null)?.let { File(it, "VanillaBTN") }

        val dirCandidates = listOfNotNull(primaryDir, altDir, appSpecificDir)
        targetDataDir = dirCandidates.firstOrNull { it.exists() } ?: primaryDir
        targetDataDir?.let {
            if (!it.exists()) {
                it.mkdirs()
            }
            try {
                Os.setenv("HMBTN_USER_DATA_DIR", it.absolutePath, true)
            } catch (_: Exception) {}
        }

        // Scan for game disc (.bin / .cue)
        val discCandidates = listOf(
            "game.cue", "game.bin",
            "hmbtn.cue", "hmbtn.bin",
            "VanillaBTN.cue", "VanillaBTN.bin"
        )

        for (dir in dirCandidates) {
            if (!dir.exists()) continue
            foundDiscFile = discCandidates.map { File(dir, it) }.firstOrNull { it.exists() }
            if (foundDiscFile == null) {
                foundDiscFile = dir.listFiles()?.firstOrNull { file ->
                    val name = file.name.lowercase()
                    (name.endsWith(".bin") || name.endsWith(".cue")) &&
                            !name.endsWith(".hmc") && !name.endsWith(".tmp")
                }
            }
            if (foundDiscFile != null) {
                targetDataDir = dir
                break
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
            showMissingDiscDialog(targetDataDir?.absolutePath ?: primaryDir.absolutePath)
        } else {
            // Attach Virtual Gamepad Overlay over SDL Surface
            setupGamepadOverlay()
        }
    }

    private fun checkStoragePermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                try {
                    val intent = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION).apply {
                        data = Uri.parse("package:$packageName")
                    }
                    startActivity(intent)
                } catch (_: Exception) {
                    try {
                        val intent = Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)
                        startActivity(intent)
                    } catch (_: Exception) {}
                }
            }
        } else {
            val permissions = mutableListOf<String>()
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                permissions.add(Manifest.permission.READ_EXTERNAL_STORAGE)
            }
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                permissions.add(Manifest.permission.WRITE_EXTERNAL_STORAGE)
            }
            if (permissions.isNotEmpty()) {
                ActivityCompat.requestPermissions(this, permissions.toTypedArray(), 100)
            }
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
        val layout = mLayout ?: return
        val overlay = VirtualGamepadView(this)
        gamepadOverlay = overlay
        val params = FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT
        )
        layout.addView(overlay, params)
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
