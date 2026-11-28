<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Factories\HasFactory;
use Illuminate\Database\Eloquent\Model;


class Perangkat extends Model
{
    
    use HasFactory;
    protected $table = 'perangkats';

    protected $primaryKey = 'id_perangkat';

    protected $fillable = [
        'nama_brankas',
        'status_pintu', 
        'status_alarm',
        'last_seen'
    ];

    protected $casts = [
        'last_seen' => 'datetime'
    ];

    public function logs(){
        return $this->hasMany(LogAktivitas::class, 'id_perangkat', 'id_perangkat');
    }

    public function resetAlarm(string $sumber = 'Dashboard Web'): LogAktivitas
    {
        $this->update([
            'status_alarm' => 'aman',
        ]);

        return LogAktivitas::create([
            'id_perangkat' => $this->id_perangkat,
            'aktivitas'    => "Reset Alarm ({$sumber})",
            'hasil'        => 'Berhasil',
        ]);
    }
}
