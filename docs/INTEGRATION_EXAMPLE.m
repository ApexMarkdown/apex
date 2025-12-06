/**
 * Example code snippets for integrating Apex into Marked
 *
 * These snippets show how to add Apex processor support to existing Marked code
 */

#import "NSString+Apex.h"

// ============================================================================
// EXAMPLE 1: Adding Apex to MKConductorTransformer.m runProcessor method
// ============================================================================

// Around line 216, add this case after kramdown:

  } else if ([processor isEqualToString:@"kramdown"]) {
    useCustom = NO;
    [defaults setBool:NO forKey:@"isMultiMarkdownDefault"];
    [defaults setValue:@"Kramdown" forKey:@"defaultProcessor"];
  } else if ([processor isEqualToString:@"apex"]) {
    useCustom = NO;
    [defaults setBool:NO forKey:@"isMultiMarkdownDefault"];
    [defaults setValue:@"Apex" forKey:@"defaultProcessor"];
  } else if ([processor isEqualToString:@"custom"]) {
    useCustom = YES;
  }

// Around line 232, add this conversion case:

  } else if ([processor isEqualToString:@"Kramdown"]) {
    result = [NSString convertWithKramdown:text];
  } else if ([processor isEqualToString:@"Apex"]) {
    result = [NSString convertWithApex:text];
  } else if ([processor isEqualToString:@"MultiMarkdown"]) {
    result = [NSString convertWithMultiMarkdown:text];
  }

// ============================================================================
// EXAMPLE 2: Adding Apex to NSString_MultiMarkdown.m processMultiMarkdown
// ============================================================================

// In the processor selection code around line 3878:

  } else if ([processor isEqualToString:@"Kramdown"]) {
    DDLogInfo(@"Starting Kramdown conversion");
    out = [NSString convertWithKramdown:safeInputString];
  } else if ([processor isEqualToString:@"Apex"]) {
    DDLogInfo(@"Starting Apex conversion");
    out = [NSString convertWithApex:safeInputString];
  } else if ([processor isEqualToString:@"MultiMarkdown"]) {
    DDLogInfo(@"Starting MultiMarkdown conversion");
    out = [self convertWithMultiMarkdown:safeInputString];
  }

// Also add Apex handling in custom processor fallback around line 3780:

  } else if ([outputString.uppercaseString isEqualToString:@"KRAMDOWN"]) {
    DDLogInfo(@"Custom processor returned KRAMDOWN directive");
    [defaults setBool:NO forKey:@"isMultiMarkdownDefault"];
    [defaults setValue:@"Kramdown" forKey:@"defaultProcessor"];
  } else if ([outputString.uppercaseString isEqualToString:@"APEX"]) {
    DDLogInfo(@"Custom processor returned APEX directive");
    [defaults setBool:NO forKey:@"isMultiMarkdownDefault"];
    [defaults setValue:@"Apex" forKey:@"defaultProcessor"];
  } else {
    // ... existing code
  }

// ============================================================================
// EXAMPLE 3: Using Apex with specific mode
// ============================================================================

// You can call Apex with a specific processor mode:
NSString *html;

// Use GFM mode
html = [NSString convertWithApex:markdown mode:@"gfm"];

// Use MultiMarkdown mode
html = [NSString convertWithApex:markdown mode:@"multimarkdown"];

// Use unified mode (all features)
html = [NSString convertWithApex:markdown mode:@"unified"];

// ============================================================================
// EXAMPLE 4: Adding to Preferences UI
// ============================================================================

// In AppPrefsWindowController.m or wherever processor dropdown is populated
// Add "Apex (Unified)" to the list of processors:

NSArray *processors = @[
    @"MultiMarkdown",
    @"Discount (GFM)",
    @"CommonMark",
    @"Kramdown",
    @"Apex"  // Add this
];

// ============================================================================
// EXAMPLE 5: Using Apex from Custom Processor Rules
// ============================================================================

// Users can create a Custom Processor Rule that returns "APEX" to use Apex:

// In a shell script custom processor:
#!/bin/bash
if [[ "$MARKED_PATH" == *.wiki ]]; then
    echo "APEX"
else
    echo "NOCUSTOM"
fi

// ============================================================================
// EXAMPLE 6: Direct C API usage (if needed for performance)
// ============================================================================

#include <apex/apex.h>

// Get default options
apex_options options = apex_options_default();

// Or get mode-specific options
apex_options gfm_options = apex_options_for_mode(APEX_MODE_GFM);

// Convert markdown
const char *markdown = "# Hello\n\nWorld";
char *html = apex_markdown_to_html(markdown, strlen(markdown), &options);

// Use html...

// Clean up
apex_free_string(html);

// ============================================================================
// Notes
// ============================================================================

/*
 * Performance: Apex should be comparable to or faster than existing processors
 * Memory: Uses cmark-gfm's efficient arena allocator
 * Thread Safety: Create separate apex_options for each thread
 * Error Handling: Returns empty string on error, never NULL
 */

