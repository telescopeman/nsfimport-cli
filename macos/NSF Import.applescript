-- NSF Import
-- Drop .nsf / .nsfe files onto this app, or double-click it and choose some.
-- Each NSF becomes a FamiTracker module (.ftm) in the same folder, with every
-- track as a subsong. This app just runs the bundled `nsfimport` command-line
-- tool with its default settings (see README.md).

on run
	try
		set theFiles to choose file with prompt "Choose NSF files to convert to FamiTracker modules:" of type {"nsf", "nsfe"} with multiple selections allowed
	on error number -128
		return -- cancelled
	end try
	convertFiles(theFiles)
end run

on open theFiles
	convertFiles(theFiles)
end open

on convertFiles(theFiles)
	set exe to POSIX path of (path to resource "nsfimport")
	set report to ""
	set lastModule to ""
	set fileCount to count of theFiles

	set progress total steps to fileCount
	set progress completed steps to 0
	set progress description to "Converting NSF files…"

	repeat with i from 1 to fileCount
		set f to item i of theFiles
		set p to POSIX path of f
		set fileName to do shell script "basename " & quoted form of p
		set progress additional description to fileName

		-- AppleScript text comparisons ignore case, so .NSF matches too
		if p ends with ".nsf" or p ends with ".nsfe" then
			if p ends with ".nsfe" then
				set baseLength to (length of p) - 5
			else
				set baseLength to (length of p) - 4
			end if
			set modulePath to (text 1 thru baseLength of p) & ".ftm"

			try
				set toolOutput to do shell script quoted form of exe & " " & quoted form of p & " 2>&1"
				set subsongs to ""
				set warnings to ""
				repeat with L in paragraphs of toolOutput
					set L to contents of L
					if L starts with "->" then
						-- "-> /path/Game.ftm (18 subsongs, 1 instrument, 0 DPCM samples)"
						set subsongs to subsongs & "(" & lastPart(L, " (")
					else if L starts with "warning" then
						set warnings to warnings & "    " & L & return
					end if
				end repeat
				set report to report & "✓ " & fileName & "  " & subsongs & return & warnings
				set lastModule to modulePath
			on error errMsg
				set report to report & "✗ " & fileName & ": " & errMsg & return
			end try
		else
			set report to report & "✗ " & fileName & ": not an .nsf or .nsfe file" & return
		end if

		set progress completed steps to i
	end repeat

	if lastModule is "" then
		display dialog report buttons {"OK"} default button "OK" with title "NSF Import" with icon caution
	else
		set reply to display dialog report buttons {"Show in Finder", "OK"} default button "OK" with title "NSF Import"
		if button returned of reply is "Show in Finder" then
			do shell script "open -R " & quoted form of lastModule
		end if
	end if
end convertFiles

-- text after the last occurrence of delim
on lastPart(t, delim)
	set oldDelims to AppleScript's text item delimiters
	set AppleScript's text item delimiters to delim
	set tailText to last text item of t
	set AppleScript's text item delimiters to oldDelims
	return tailText
end lastPart
