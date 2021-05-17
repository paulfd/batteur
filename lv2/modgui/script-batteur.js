
function (event) {
	function handle_event (event) {
		switch (event.symbol) {
			case 'status':
				switch(event.value) {
					case 0:
						event.icon.find('[mod-role=batteur-status]').text('Stopped');
						break;
					case 1:
						event.icon.find('[mod-role=batteur-status]').text('Intro');
						break;
					case 2:
						event.icon.find('[mod-role=batteur-status]').text('Playing');
						break;
					case 3:
						event.icon.find('[mod-role=batteur-status]').text('Fill-in');
						break;
					case 4:
						event.icon.find('[mod-role=batteur-status]').text('Transition');
						break;
					case 5:
						event.icon.find('[mod-role=batteur-status]').text('Ending');
						break;
				}
				break;
			case 'parttotal':
				if (event.value == 0)
					event.icon.find('[mod-role=batteur-part-total]').text('-');
				else
					event.icon.find('[mod-role=batteur-part-total]').text(event.value);
				break;
			case 'partindex':
				if (event.value == 0)
					event.icon.find('[mod-role=batteur-part-index]').text('-');
				else
					event.icon.find('[mod-role=batteur-part-index]').text(event.value);
				break;
			case 'filltotal':
				if (event.value == 0)
					event.icon.find('[mod-role=batteur-fill-total]').text('-');
				else
					event.icon.find('[mod-role=batteur-fill-total]').text(event.value);
				break;
			case 'fillindex':
				if (event.value == 0)
					event.icon.find('[mod-role=batteur-fill-index]').text('-');
				else
					event.icon.find('[mod-role=batteur-fill-index]').text(event.value);
				break;
			case 'timenum':
				if (event.value == 0)
					event.icon.find('[mod-role=batteur-signature-num]').text('-');
				else
					event.icon.find('[mod-role=batteur-signature-num]').text(event.value);

				var canvas = document.getElementById("batteur-pos-canvas");
				if (canvas.getContext) {
					var context = canvas.getContext("2d");
					let beatWidth = (canvas.width - 2) / event.value;
					context.clearRect(0, 0, canvas.width, canvas.height);
					for (var i = 0; i <= event.value; i++) {
						context.beginPath();
						context.moveTo(beatWidth * i + 1, 0);
						context.lineTo(beatWidth * i + 1, 8);
						context.stroke();
						context.beginPath();
						context.moveTo(beatWidth * i + 1, 32);
						context.lineTo(beatWidth * i + 1, 40);
						context.stroke();
					}
				}
				break;
			case 'timedenom':
				if (event.value == 0)
					event.icon.find('[mod-role=batteur-signature-denom]').text('-');
				else
					event.icon.find('[mod-role=batteur-signature-denom]').text(event.value);
				break;
			case 'barpos':
				var canvas = document.getElementById("batteur-pos-canvas");
				var text = event.icon.find('[mod-role=batteur-signature-num]').text();
				if (text == '-')
					break;
				
				var timenum = parseInt(text);
				if (timenum <= 0)
					break;

				if (canvas.getContext) {
					var context = canvas.getContext("2d");
					let beatWidth = canvas.width / timenum;
					let width = event.value * beatWidth;
					context.clearRect(0, 10, canvas.width, 20);
					context.fillRect(0, 10, width, 20);
				}
				break;
			default:
				break;
		}
	}

	if (event.type == 'change') {
		handle_event(event);
	}
}