package com.telemetry.security

import com.telemetry.config.SecurityConfig
import jakarta.servlet.FilterChain
import jakarta.servlet.http.HttpServletRequest
import jakarta.servlet.http.HttpServletResponse
import org.springframework.security.authentication.UsernamePasswordAuthenticationToken
import org.springframework.security.core.context.SecurityContextHolder
import org.springframework.security.web.authentication.WebAuthenticationDetailsSource
import org.springframework.stereotype.Component
import org.springframework.web.filter.OncePerRequestFilter

@Component
class ApiKeyAuthenticationFilter(
    private val telemetryConfig: com.telemetry.config.TelemetryConfig
) : OncePerRequestFilter() {

    companion object {
        const val API_KEY_HEADER = "X-API-Key"
        const val API_KEY_PARAM = "apiKey"
    }

    override fun doFilterInternal(
        request: HttpServletRequest,
        response: HttpServletResponse,
        filterChain: FilterChain
    ) {
        val apiKey = extractApiKey(request)
        
        if (apiKey != null && isValidApiKey(apiKey)) {
            val authentication = UsernamePasswordAuthenticationToken(
                apiKey, null, emptyList()
            )
            authentication.details = WebAuthenticationDetailsSource().buildDetails(request)
            SecurityContextHolder.getContext().authentication = authentication
        }
        
        filterChain.doFilter(request, response)
    }

    private fun extractApiKey(request: HttpServletRequest): String? {
        // Try header first, then query parameter
        return request.getHeader(API_KEY_HEADER) 
            ?: request.getParameter(API_KEY_PARAM)
    }

    private fun isValidApiKey(apiKey: String): Boolean {
        return telemetryConfig.security.apiKeys.contains(apiKey)
    }
}