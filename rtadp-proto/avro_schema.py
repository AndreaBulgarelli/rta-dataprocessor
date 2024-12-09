AVRO_SCHEMA = '''
			{
				"type": "record",
				"name": "AvroMonitoringPoint",
				"namespace": "astri.mon.kafka",
				"fields": [
					{"name": "assembly", "type": "string"},
					{"name": "name", "type": "string"},
					{"name": "serial_number", "type": "string"},
					{"name": "timestamp", "type": "double"},
					{"name": "source_timestamp", "type": ["null", "long"]},
					{"name": "units", "type": "string"},
					{"name": "archive_suppress", "type": "boolean"},
					{"name": "env_id", "type": "string"},
					{"name": "eng_gui", "type": "boolean"},
					{"name": "op_gui", "type": "boolean"},
					{"name": "data", "type": {"type": "array", "items": ["double", "int", "long", "string", "boolean"]}}
				]
			}
		'''