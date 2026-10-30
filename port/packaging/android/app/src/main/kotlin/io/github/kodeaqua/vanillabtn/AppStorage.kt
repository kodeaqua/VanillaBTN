package io.github.kodeaqua.vanillabtn

import android.content.Context
import java.io.File
import java.io.IOException

object AppStorage {
    @Throws(IOException::class)
    fun ensureUserDataDir(context: Context): File {
        val externalFilesDir = try {
            context.getExternalFilesDir(null)
        } catch (_: SecurityException) {
            null
        }
        val candidates = listOfNotNull(
            externalFilesDir?.let { File(it, "VanillaBTN") },
            File(context.filesDir, "VanillaBTN")
        ).distinctBy { it.absolutePath }

        for (directory in candidates) {
            if (ensureWritableDirectory(directory) &&
                ensureWritableDirectory(File(directory, "saves"))) {
                return directory
            }
        }

        throw IOException("No writable app-private storage directory is available")
    }

    private fun ensureWritableDirectory(directory: File): Boolean {
        return try {
            if (!directory.exists() && !directory.mkdirs()) return false
            if (!directory.isDirectory) return false
            val probe = File.createTempFile("vbtn", ".tmp", directory)
            probe.delete()
        } catch (_: IOException) {
            false
        } catch (_: SecurityException) {
            false
        }
    }
}
