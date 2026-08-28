package com.example.mobileclock

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.sp
import com.example.mobileclock.ui.theme.MobileClockTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {
            MobileClockTheme {
                HelloWorldScreen()
            }
        }
    }
}

@Composable
fun HelloWorldScreen() {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFF666666)),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = "HelloWorld",
            color = Color(0xFFFFEB3B),
            fontSize = 36.sp
        )
    }
}