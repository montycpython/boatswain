package bosun.ops

import android.app.NativeActivity
import android.app.AlertDialog
import android.view.inputmethod.InputMethodManager
import android.content.Context
import android.view.ViewGroup
import android.widget.EditText
import android.widget.FrameLayout
import android.graphics.Color
import android.view.View
import android.os.Bundle


class BosunActivity : NativeActivity() {
    private lateinit var editText: EditText
    private lateinit var overlayLayout: FrameLayout
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setupOverlay()
    }
    fun toggleKeyboard(show: Boolean) {
        runOnUiThread {
            val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
            val view = window.decorView
            
            if (show) {
                view.isFocusable = true
                view.isFocusableInTouchMode = true
                view.requestFocus()
                
                imm.showSoftInput(view, InputMethodManager.SHOW_IMPLICIT)
            } else {
                imm.hideSoftInputFromWindow(view.windowToken, 0)
            }
        }
    }
    fun showBoatswainDialog() {
        runOnUiThread {
            AlertDialog.Builder(this)
                .setMessage("Boatswain ⛴️ by Monty C. Python\nHealth, Wealth, and Relationships")
                .setPositiveButton("Aye Aye!") { dialog: android.content.DialogInterface, _ -> dialog.dismiss() }
                .show()
        }
    }
    private fun setupOverlay() {
        runOnUiThread {
            overlayLayout = FrameLayout(this)
            editText = EditText(this).apply {
                setBackgroundColor(Color.WHITE)
                setTextColor(Color.BLACK)
                visibility = View.GONE
                // Set fixed or dynamic size
                layoutParams = FrameLayout.LayoutParams(600, 120)
            }
            
            val params = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
            addContentView(overlayLayout, params)
            overlayLayout.addView(editText)
        }
    }
    fun showEditableText(x: Float, y: Float, initialText: String) {
        runOnUiThread {
            editText.apply {
                translationX = x
                translationY = y
                setText(initialText)
                visibility = View.VISIBLE
                requestFocus()
                val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                imm.showSoftInput(this, InputMethodManager.SHOW_IMPLICIT)
            }
        }
    }
    fun getTypedText(): String {
        return editText.text.toString()
    }
}

//------------>>>>>>>>>>>>
