package io.github.kodeaqua.vanillabtn

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.os.Environment
import android.provider.OpenableColumns
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.google.android.material.button.MaterialButton
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.progressindicator.LinearProgressIndicator
import java.io.File
import java.io.FileOutputStream

class LauncherActivity : AppCompatActivity() {

    private lateinit var progressContainer: LinearLayout
    private lateinit var progressBar: LinearProgressIndicator
    private lateinit var progressText: TextView
    private lateinit var btnSelectDisc: MaterialButton

    private val discPicker = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK && result.data != null) {
            val uri = result.data?.data
            if (uri != null) {
                processSelectedDisc(uri)
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Enforce sticky immersive full screen landscape
        WindowCompat.setDecorFitsSystemWindows(window, false)
        val controller = WindowInsetsControllerCompat(window, window.decorView)
        controller.hide(WindowInsetsCompat.Type.systemBars())
        controller.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE

        val safeDir = File(getExternalFilesDir(null), "VanillaBTN")
        if (!safeDir.exists()) safeDir.mkdirs()
        File(safeDir, "saves").mkdirs()

        // 1. Check if game disc already exists in safe storage
        val discCandidates = listOf("game.bin", "hmbtn.bin", "VanillaBTN.bin")
        val safeDisc = discCandidates.map { File(safeDir, it) }
            .firstOrNull { it.exists() && it.length() > 10 * 1024 * 1024 }

        if (safeDisc != null) {
            // Already installed: launch game immediately with zero delay
            launchGame()
            return
        }

        // 2. Check if game disc exists in legacy Documents folder
        val legacyDocs = File(
            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS),
            "VanillaBTN"
        )
        val legacyDisc = discCandidates.map { File(legacyDocs, it) }
            .firstOrNull { it.exists() && it.length() > 10 * 1024 * 1024 }

        if (legacyDisc != null) {
            // Legacy disc found, import it seamlessly
            setContentView(R.layout.activity_setup)
            initViews()
            importLegacyFile(legacyDisc, File(safeDir, "game.bin"))
            return
        }

        // 3. Fresh install: display setup screen
        setContentView(R.layout.activity_setup)
        initViews()
    }

    private fun initViews() {
        progressContainer = findViewById(R.id.progressContainer)
        progressBar = findViewById(R.id.progressBar)
        progressText = findViewById(R.id.progressText)
        btnSelectDisc = findViewById(R.id.btnSelectDisc)

        btnSelectDisc.setOnClickListener {
            val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                type = "*/*"
                putExtra(
                    Intent.EXTRA_MIME_TYPES,
                    arrayOf("application/octet-stream", "application/x-iso9660-image", "*/*")
                )
            }
            discPicker.launch(intent)
        }
    }

    private fun processSelectedDisc(uri: Uri) {
        var displayName = "game.bin"
        var fileSize = 0L

        try {
            contentResolver.query(uri, null, null, null, null)?.use { cursor ->
                val nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                val sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE)
                if (cursor.moveToFirst()) {
                    if (nameIndex != -1) displayName = cursor.getString(nameIndex)
                    if (sizeIndex != -1) fileSize = cursor.getLong(sizeIndex)
                }
            }
        } catch (_: Exception) {}

        if (displayName.lowercase().endsWith(".cue")) {
            MaterialAlertDialogBuilder(this)
                .setTitle(R.string.game_disc_not_found_title)
                .setMessage("File yang dipilih adalah .CUE (daftar trek). Silakan pilih file .BIN utama game (biasanya berukuran sekitar 400 - 600 MB).")
                .setPositiveButton("OK", null)
                .show()
            return
        }

        progressContainer.visibility = View.VISIBLE
        btnSelectDisc.isEnabled = false
        progressBar.isIndeterminate = fileSize <= 0
        if (fileSize > 0) {
            progressBar.max = 100
            progressBar.progress = 0
        }

        val safeDir = File(getExternalFilesDir(null), "VanillaBTN")
        val targetTemp = File(safeDir, "game.bin.tmp")
        val targetFinal = File(safeDir, "game.bin")

        Thread {
            try {
                contentResolver.openInputStream(uri)?.use { input ->
                    FileOutputStream(targetTemp).use { output ->
                        val buffer = ByteArray(65536)
                        var bytesRead: Int
                        var totalRead = 0L

                        while (input.read(buffer).also { bytesRead = it } != -1) {
                            output.write(buffer, 0, bytesRead)
                            totalRead += bytesRead

                            if (fileSize > 0) {
                                val percent = (totalRead * 100 / fileSize).toInt()
                                runOnUiThread {
                                    progressBar.progress = percent
                                    val mbRead = totalRead / (1024 * 1024)
                                    val mbTotal = fileSize / (1024 * 1024)
                                    progressText.text = "${getString(R.string.setup_copying)} ($mbRead MB / $mbTotal MB - $percent%)"
                                }
                            }
                        }
                        output.flush()
                    }
                }

                if (targetTemp.length() < 10 * 1024 * 1024) {
                    targetTemp.delete()
                    runOnUiThread {
                        progressContainer.visibility = View.GONE
                        btnSelectDisc.isEnabled = true
                        Toast.makeText(this, "Ukuran file terlalu kecil atau tidak valid.", Toast.LENGTH_LONG).show()
                    }
                    return@Thread
                }

                if (targetFinal.exists()) targetFinal.delete()
                targetTemp.renameTo(targetFinal)

                runOnUiThread {
                    launchGame()
                }
            } catch (e: Exception) {
                targetTemp.delete()
                runOnUiThread {
                    progressContainer.visibility = View.GONE
                    btnSelectDisc.isEnabled = true
                    Toast.makeText(this, getString(R.string.setup_copy_failed), Toast.LENGTH_LONG).show()
                }
            }
        }.start()
    }

    private fun importLegacyFile(source: File, targetFinal: File) {
        progressContainer.visibility = View.VISIBLE
        btnSelectDisc.isEnabled = false
        val fileSize = source.length()
        progressBar.max = 100
        progressBar.progress = 0

        val targetTemp = File(targetFinal.parentFile, "game.bin.tmp")

        Thread {
            try {
                source.inputStream().use { input ->
                    FileOutputStream(targetTemp).use { output ->
                        val buffer = ByteArray(65536)
                        var bytesRead: Int
                        var totalRead = 0L

                        while (input.read(buffer).also { bytesRead = it } != -1) {
                            output.write(buffer, 0, bytesRead)
                            totalRead += bytesRead

                            val percent = (totalRead * 100 / fileSize).toInt()
                            runOnUiThread {
                                progressBar.progress = percent
                                val mbRead = totalRead / (1024 * 1024)
                                val mbTotal = fileSize / (1024 * 1024)
                                progressText.text = "Mengimpor dari Documents ($mbRead MB / $mbTotal MB - $percent%)"
                            }
                        }
                        output.flush()
                    }
                }

                if (targetFinal.exists()) targetFinal.delete()
                targetTemp.renameTo(targetFinal)

                runOnUiThread {
                    launchGame()
                }
            } catch (_: Exception) {
                targetTemp.delete()
                runOnUiThread {
                    progressContainer.visibility = View.GONE
                    btnSelectDisc.isEnabled = true
                }
            }
        }.start()
    }

    private fun launchGame() {
        val intent = Intent(this, MainActivity::class.java)
        intent.addFlags(Intent.FLAG_ACTIVITY_FORWARD_RESULT)
        startActivity(intent)
        finish()
        overridePendingTransition(0, 0)
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
