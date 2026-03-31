# ESP Analytics Component Makefile
# Helper targets for testing and development

.PHONY: test-syntax clean-docs check-deps

# Test syntax by checking header file
test-syntax:
	@echo "Checking component syntax..."
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --quiet --error-exitcode=1 --enable=all \
		--std=c99 include/esp_analytics.h esp_analytics.c; \
	else \
		echo "cppcheck not found, skipping syntax check"; \
	fi

# Check dependencies
check-deps:
	@echo "Checking component dependencies..."
	@echo "Required components:"
	@echo "  - esp_http_client"
	@echo "  - nvs_flash"
	@echo "  - freertos"
	@echo "  - esp_log"
	@echo "  - esp_wifi"
	@echo "  - esp_sleep"
	@echo "  - esp_timer"
	@echo "  - cjson"

# Generate documentation
docs:
	@echo "Documentation is available in README.md and INTEGRATION_GUIDE.md"

# Clean temporary files
clean:
	@echo "Cleaning component..."
	@rm -f *.o *.a

# Show component info
info:
	@echo "ESP Analytics Component"
	@echo "======================="
	@echo "Files:"
	@ls -la
	@echo ""
	@echo "Size:"
	@du -sh .

# Verify component structure
verify:
	@echo "Verifying component structure..."
	@test -f include/esp_analytics.h || (echo "Missing header file" && exit 1)
	@test -f esp_analytics.c || (echo "Missing source file" && exit 1)
	@test -f CMakeLists.txt || (echo "Missing CMakeLists.txt" && exit 1)
	@test -f Kconfig || (echo "Missing Kconfig" && exit 1)
	@test -f README.md || (echo "Missing README.md" && exit 1)
	@echo "Component structure verified!"

# All checks
check: verify check-deps test-syntax
	@echo "All checks completed!"

help:
	@echo "Available targets:"
	@echo "  test-syntax  - Check C syntax with cppcheck"
	@echo "  check-deps   - Show required dependencies"
	@echo "  docs         - Show documentation info"
	@echo "  clean        - Clean temporary files"
	@echo "  info         - Show component information"
	@echo "  verify       - Verify component structure"
	@echo "  check        - Run all checks"
	@echo "  help         - Show this help"
