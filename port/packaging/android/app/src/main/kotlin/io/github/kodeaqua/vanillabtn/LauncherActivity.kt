package io.github.kodeaqua.vanillabtn

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
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
import java.io.IOException
import java.nio.file.AtomicMoveNotSupportedException
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.util.Locale

class LauncherActivity : AppCompatActivity() {

    private lateinit var progressContainer: LinearLayout
    private lateinit var progressBar: LinearProgressIndicator
    private lateinit var progressText: TextView
    private lateinit var btnSelectDisc: MaterialButton
    private var safeDataDir: File? = null

    private companion object {
        const val MIN_DISC_BYTES = 10L * 1024L * 1024L
    }

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

        safeDataDir = try {
            AppStorage.ensureUserDataDir(this)
        } catch (_: Exception) {
            null
        }

        val safeDir = safeDataDir
        if (safeDir != null) {
            val discCandidates = listOf("game.bin", "hmbtn.bin", "VanillaBTN.bin")
            val safeDisc = discCandidates.map { File(safeDir, it) }
                .firstOrNull { it.isFile && it.length() > MIN_DISC_BYTES }
            if (safeDisc != null) {
                launchGame()
                return
            }
        }

        setContentView(R.layout.activity_setup)
        initViews()
        if (safeDir == null) {
            Toast.makeText(this, getString(R.string.setup_storage_unavailable), Toast.LENGTH_LONG).show()
        }
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
                    if (nameIndex != -1) {
                        cursor.getString(nameIndex)?.takeIf { it.isNotBlank() }?.let { displayName = it }
                    }
                    if (sizeIndex != -1) fileSize = cursor.getLong(sizeIndex)
                }
            }
        } catch (_: Exception) {}

        if (!displayName.lowercase(Locale.ROOT).endsWith(".bin")) {
            MaterialAlertDialogBuilder(this)
                .setTitle(R.string.game_disc_not_found_title)
                .setMessage(R.string.setup_select_raw_bin)
                .setPositiveButton("OK", null)
                .show()
            return
        }

        val safeDir = safeDataDir ?: try {
            AppStorage.ensureUserDataDir(this).also { safeDataDir = it }
        } catch (_: Exception) {
            Toast.makeText(this, getString(R.string.setup_storage_unavailable), Toast.LENGTH_LONG).show()
            return
        }

        progressContainer.visibility = View.VISIBLE
        btnSelectDisc.isEnabled = false
        progressBar.isIndeterminate = fileSize <= 0
        if (fileSize > 0) {
            progressBar.max = 100
            progressBar.progress = 0
        }

        val targetTemp = File(safeDir, "game.bin.tmp")
        val targetFinal = File(safeDir, "game.bin")

        Thread {
            try {
                val input = contentResolver.openInputStream(uri)
                    ?: throw IOException("Could not open the selected document")
                input.use { discInput ->
                    FileOutputStream(targetTemp).use { output ->
                        val buffer = ByteArray(65536)
                        var bytesRead: Int
                        var totalRead = 0L

                        while (discInput.read(buffer).also { bytesRead = it } != -1) {
                            output.write(buffer, 0, bytesRead)
                            totalRead += bytesRead

                            if (fileSize > 0) {
                                val percent = (totalRead * 100 / fileSize).coerceIn(0, 100).toInt()
                                runOnUiThread {
                                    progressBar.progress = percent
                                    val mbRead = totalRead / (1024 * 1024)
                                    val mbTotal = fileSize / (1024 * 1024)
                                    progressText.text = "${getString(R.string.setup_copying)} ($mbRead MB / $mbTotal MB - $percent%)"
                                }
                            }
                        }
                        output.flush()
                        output.fd.sync()
                    }
                }

                if (targetTemp.length() < MIN_DISC_BYTES) {
                    targetTemp.delete()
                    runOnUiThread {
                        progressContainer.visibility = View.GONE
                        btnSelectDisc.isEnabled = true
                        Toast.makeText(this, getString(R.string.setup_invalid_disc), Toast.LENGTH_LONG).show()
                    }
                    return@Thread
                }
                if (fileSize > 0 && targetTemp.length() != fileSize) {
                    throw IOException("The copied document length did not match its reported size")
                }

                moveReplacing(targetTemp, targetFinal)

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

    private fun moveReplacing(source: File, target: File) {
        try {
            Files.move(
                source.toPath(),
                target.toPath(),
                StandardCopyOption.ATOMIC_MOVE,
                StandardCopyOption.REPLACE_EXISTING
            )
        } catch (_: AtomicMoveNotSupportedException) {
            Files.move(source.toPath(), target.toPath(), StandardCopyOption.REPLACE_EXISTING)
        }
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
