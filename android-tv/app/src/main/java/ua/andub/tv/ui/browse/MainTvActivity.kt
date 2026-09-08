package ua.andub.tv.ui.browse

import android.os.Bundle
import androidx.fragment.app.FragmentActivity

class MainTvActivity : FragmentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (savedInstanceState == null) {
            supportFragmentManager.beginTransaction()
                .replace(android.R.id.content, MainTvFragment())
                .commit()
        }
    }
}
