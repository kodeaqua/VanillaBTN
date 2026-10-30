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
import android.util.Log
import android.view.KeyEvent
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
            val dir = targetDataDir ?: File(
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS),
                "VanillaBTN"
            )
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

        // Request storage permissions if needed
        checkStoragePermissions()

        // Resolve user data directory
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
                log("HMBTN_USER_DATA_DIR set to: ${it.absolutePath}")
            } catch (e: Exception) {
                log("Failed to set HMBTN_USER_DATA_DIR: ${e.message}")
            }
        }

        // Scan for game disc (.bin / .cue)
        val discCandidates = listOf(
            "game.bin", "hmbtn.bin", "VanillaBTN.bin",
            "game.cue", "hmbtn.cue", "VanillaBTN.cue"
        )

        for (dir in dirCandidates) {
            if (!dir.exists()) continue
            log("Scanning directory: ${dir.absolutePath}")
            val dirFiles = dir.listFiles()
            if (dirFiles != null) {
                log("Files in directory: ${dirFiles.joinToString { it.name }}")
            } else {
                log("Directory returned null for listFiles() (Permission issue?)")
            }

            var candidateFile: File? = discCandidates.map { File(dir, it) }.firstOrNull { it.exists() }
            if (candidateFile == null) {
                candidateFile = dirFiles?.firstOrNull { file ->
                    val name = file.name.lowercase()
                    (name.endsWith(".bin") || name.endsWith(".cue")) &&
                            !name.endsWith(".hmc") && !name.endsWith(".tmp")
                }
            }

            if (candidateFile != null) {
                // If it's a .cue, attempt to resolve the actual .bin
                if (candidateFile.name.lowercase().endsWith(".cue")) {
                    val resolvedBin = resolveBinFromCue(candidateFile)
                    if (resolvedBin != null) {
                        log("Resolved .cue file (${candidateFile.name}) to binary: ${resolvedBin.absolutePath}")
                        candidateFile = resolvedBin
                    } else {
                        log("Warning: .cue file found (${candidateFile.name}) but could not locate referenced .bin in same directory")
                    }
                }
                foundDiscFile = candidateFile
                targetDataDir = dir
                try {
                    Os.setenv("HMBTN_USER_DATA_DIR", dir.absolutePath, true)
                } catch (e: Exception) {
                    log("Failed to update HMBTN_USER_DATA_DIR: ${e.message}")
                }
                log("Found disc file: ${foundDiscFile?.absolutePath}")
                break
            }
        }

        if (foundDiscFile != null) {
            try {
                Os.setenv("HMBTN_DISC_PATH", foundDiscFile!!.absolutePath, true)
                log("HMBTN_DISC_PATH set to: ${foundDiscFile!!.absolutePath}")
            } catch (e: Exception) {
                log("Failed to set HMBTN_DISC_PATH: ${e.message}")
            }
        } else {
            log("No game disc file found in candidate directories.")
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

    private fun resolveBinFromCue(cueFile: File): File? {
        try {
            val lines = cueFile.readLines()
            for (line in lines) {
                val trimmed = line.trim()
                if (trimmed.startsWith("FILE", ignoreCase = true)) {
                    val start = trimmed.indexOf('"')
                    val end = trimmed.lastIndexOf('"')
                    if (start in 0 until end) {
                        val binName = trimmed.substring(start + 1, end)
                        val binFile = File(cueFile.parentFile, binName)
                        if (binFile.exists()) return binFile
                    }
                }
            }
        } catch (e: Exception) {
            log("Error parsing cue file: ${e.message}")
        }
        return null
    }

    private fun checkStoragePermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                log("Requesting MANAGE_APP_ALL_FILES_ACCESS_PERMISSION")
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
                log("Requesting runtime storage permissions: $permissions")
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
