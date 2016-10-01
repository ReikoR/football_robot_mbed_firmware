var gulp = require('gulp');
var spawn = require('child_process').spawn;
var fs = require('fs');
var path = require('path');
var drivelist = require('drivelist');

function getDrives(callback) {
    var validDisks = [];

    drivelist.list(function(error, disks) {
        if (error) {
            console.log(error);
        }

        //console.log(disks);

        disks.forEach(function (disk) {
            if (/mbed/gi.exec(disk.description)) {
                validDisks.push(disk);
            }
        });

        callback(validDisks);
    });
}

function copyBin(callback) {
    getDrives(function (drives) {
        //console.log('---');
        //console.log(drives);

        if (drives.length > 0) {
            var targetPath = drives[0].mountpoint + '//test.bin';

            copyFile('.build/LPC1768/GCC_ARM/football_robot_mbed_firmware.bin', targetPath, callback);
        } else {
            console.log('No drives found');

            callback();
        }
    });
}

gulp.task('compile', function(callback) {
    var child = spawn('mbed', [
        'compile',
        '-t', 'GCC_ARM',
        '-m', 'LPC1768'
    ]);

    child.stdout.on('data', (data) => {
        console.log(data.toString());
    });

    child.stderr.on('data', (data) => {
        console.log(`stderr: ${data}`);
    });

    child.on('close', (code) => {
        //console.log(`child process exited with code ${code}`);
        callback(code === 0 ? null : "Compile failed");
    });
});

gulp.task('program', function(callback) {
    copyBin(callback);
});

gulp.task('compile-program', ['compile'], function(callback) {
    copyBin(callback);
});

gulp.task('run', ['compile-program']);

/*function copyFile(source, target, cb) {
    var cbCalled = false;

    var rd = fs.createReadStream(source, {
        encoding: "binary"
    });

    rd.on("error", function(err) {
        console.log(err);
        done(err);
    });

    var wr = fs.createWriteStream(target, {
        encoding: "binary"
    });

    wr.on("error", function(err) {
        console.log(err);
        done(err);
    });

    wr.on("close", function(ex) {
        done();
    });

    rd.pipe(wr);

    function done(err) {
        if (!cbCalled) {
            cb(err);
            cbCalled = true;
        }
    }
}*/

/*function copyFile(source, target, cb) {
    fs.readFile(source, {encoding: "binary"}, function (err, data) {
        if (err) {
            cb(err);
        } else {
            fs.writeFile(target, data, {encoding: "binary"}, function (err) {
                if (err) {
                    cb(err);
                } else {
                    cb();
                }
            });
        }
    });
}*/

function copyFile(source, target, cb) {
    console.log('Copying from', source, 'to', target);

    var child = spawn('copy', [
        path.normalize(source), path.normalize(target),
        '/Y'
    ], {shell: true});

    child.stdout.on('data', (data) => {
        console.log(data.toString());
    });

    child.stderr.on('data', (data) => {
        console.log(`stderr: ${data}`);
    });

    child.on('error', (error) => {
        console.log(error);
    });

    child.on('close', (code) => {
        //console.log(code);
        cb(code === 0 ? null : "Copy failed");
    });
}