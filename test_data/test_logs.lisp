
; "Iterates through a list of logs and returns all found telephone numbers."
(defun extract-phone-numbers (log-list)
  (let ((phone-regex "\\(?\\d{3}\\)?[-.\\s]?\\d{3}[-.\\s]?\\d{4}"))
    (loop for entry in log-list
          ;; SCAN-TO-STRINGS finds the first match and returns it as a string
          for match = (cl-ppcre:scan-to-strings phone-regex entry)
          when match
          collect match)))

;; Sample log data
(defparameter *log-entries*
  '("2024-04-09 10:01:00 INFO User logged in from 192.168.1.1"
    "2024-04-09 10:05:22 WARN Contact support at 555-0199 for help" ; Invalid (too short)
    "2024-04-09 10:10:45 ERROR Connection lost, call (555) 123-4567"
    "2024-04-09 10:15:00 INFO Follow up with 555.888.9999 tomorrow"))

;; Execution
(print (extract-phone-numbers *log-entries*))
;; Output: ("(555) 123-4567" "555.888.9999")
