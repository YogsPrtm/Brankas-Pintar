<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Factories\HasFactory;
use Illuminate\Database\Eloquent\Model;

class LogAktivitas extends Model
{

    use HasFactory;

    protected $table = 'log_aktivitas';

    protected $primaryKey = 'id_logs';

    protected $fillable = [
        'id_perangkat',
        'aktivitas',
        'sumber',
        'hasil'
    ];

    public function perangkat(){
        return $this->belongsTo(Perangkat::class, 'id_perangkat', 'id_perangkat');
    }
}
